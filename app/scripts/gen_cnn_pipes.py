#!/usr/bin/env python3
# Emby5 — Emby for PS5
# SPDX-License-Identifier: GPL-3.0-or-later
"""
gen_cnn_pipes.py - the "Film" (FSRCNNX) and "Anime" (ArtCNN) upscaler pipes.

Turns the original mpv user shaders in app/engine/shaders/cnn/ into AGC pipe
headers in app/engine/shaders/agc/, the same kind of header the Anime4K passes
use (upscale_a4k_*_pipe.h), so the runtime runs them through agc_up_pass():

  FSRCNNX_x2_8-0-4-1.glsl  (igv, LGPL-3.0-or-later)  -> upscale_fsrcnnx_p<k>_pipe.h
  ArtCNN_C4F8.glsl         (Artoriuz, MIT)            -> upscale_artcnn_p<k>_pipe.h
  both                                                -> upscale_cnn_pipes.h / .inc

Every hook pass of the original (one 4-channel output each) becomes one pipe.
The weights are read from the shader text and compiled in as literals, as
amdllpc does for the Anime4K passes. Two things differ from the originals,
because Emby5 upscales RGB, not a luma plane:

  - the first passes read the source picture (scratch L0, RGBA8) and take its
    BT.709 luma, which is the Y the YUV pass converted from;
  - the last pass, the sub-pixel conv, writes the network's 2x luma as a
    RESIDUAL against the bilinear source luma at each of the four sub-pixel
    centres, in the depth-to-space layout upscale_a4k_final reads (channel =
    2 * (y & 1) + (x & 1)). upscale_a4k_final then adds it to the bilinear RGB
    at the final size: at exactly 2x that puts the network's luma in place
    and keeps the chroma; beyond 2x (720p to 4K) the 2x result is scaled the
    rest of the way bilinearly, as Anime4K's is.

The pixel stage is written here in LLVM IR and compiled with the system's llc
for gfx1013 (wave64, the PAL ABI): the same user-data layout and interpolant
the Anime4K pixel shaders have (s1 = descriptor table, s2 = prim mask, v0/v1 =
the centre barycentrics of vUV; texture k's T# at 48 * k, its S# at 48 * k +
32). Everything except the pixel ISA and the VGPR/SGPR fields of
SPI_SHADER_PGM_RSRC1_PS - the shared vertex stage, every context register,
the user-SGPR counts - is copied from upscale_a4k_final_pipe.h, which every
upscale pipe shares. Texels are fetched with image_load at the pixel's own
coordinate clamped to the edge (what Anime4K's passes do), so no pass needs
WQM or derivatives.

The surfaces each pass reads and writes are planned here too (pool slots: 0 is
L0, 1..6 the RGBA16F maps of the runtime's fixed scratch pool) and written to
upscale_cnn_pipes.h for agc_upscale_run_cnn().

  python3 app/scripts/gen_cnn_pipes.py [--llc llc-18] [--check]

--check only parses the networks and prints the plan and the per-pixel cost.
"""

import argparse
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
APP = os.path.dirname(HERE)
SRC_DIR = os.path.join(APP, "engine", "shaders", "cnn")
OUT_DIR = os.path.join(APP, "engine", "shaders", "agc")
TEMPLATE = os.path.join(OUT_DIR, "upscale_a4k_final_pipe.h")

# BT.709 luma, the matrix the YUV pass converts HD video with.
LUMA = (0.2126, 0.7152, 0.0722)

NETWORKS = [
    # (prefix, source, pipe id base, description)
    ("fsrcnnx", "FSRCNNX_x2_8-0-4-1.glsl", "EVO_AGC_PIPE_UP_FSRCNNX0",
     "FSRCNNX x2 8-0-4-1 (igv/FSRCNN-TensorFlow 1.1, LGPL-3.0-or-later)"),
    ("artcnn", "ArtCNN_C4F8.glsl", "EVO_AGC_PIPE_UP_ARTCNN0",
     "ArtCNN C4F8 (Artoriuz/ArtCNN 3e40fe3, MIT)"),
]

POOL_SLOTS = 6   # RGBA16F maps in the runtime's pool (EVO_AGC_UP_POOL_FP16_SLOTS)

# --------------------------------------------------------------------------
# The mpv hook files
# --------------------------------------------------------------------------

FLOAT = r"[-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?"


def floats(text):
    return [float(x) for x in re.findall(FLOAT, text)]


def hook_blocks(path):
    """The passes of an mpv user shader: directives and body, in order."""
    blocks, cur = [], None
    with open(path, encoding="utf-8") as f:
        for line in f:
            s = line.strip()
            if s.startswith("//!"):
                key, _, val = s[3:].partition(" ")
                if cur is None or cur["body"]:
                    cur = {"dir": [], "body": []}
                    blocks.append(cur)
                cur["dir"].append((key, val.strip()))
            elif cur is not None and s and not s.startswith("//"):
                cur["body"].append(s)
    for b in blocks:
        b["desc"] = next((v for k, v in b["dir"] if k == "DESC"), "")
        b["binds"] = [v for k, v in b["dir"] if k == "BIND"]
        b["save"] = next((v for k, v in b["dir"] if k == "SAVE"), None)
        b["compute"] = any(k == "COMPUTE" for k, v in b["dir"])
        b["code"] = " ".join(b["body"])
    return blocks


class Term:
    """out += W * sum(inputs at their offsets). W is out x in (in = 1 for luma)."""

    def __init__(self, w, srcs):
        self.w = w          # list of 4 rows
        self.srcs = srcs    # [(texname, dx, dy)]


class Pass:
    def __init__(self, name, binds, save):
        self.name, self.binds, self.save = name, binds, save
        self.bias = [0.0] * 4
        self.terms = []
        self.act = None     # None | ("relu",) | ("prelu", alpha[4])
        self.final = None   # set on the last pass: dict(order=..., clamp=...)


def mat4_rows(vals):
    """GLSL mat4(...) is column-major: M * v gives out[i] = sum_j m[4 j + i] v[j]."""
    assert len(vals) == 16, vals
    return [[vals[4 * j + i] for j in range(4)] for i in range(4)]


def parse_offset(text):
    t = text.strip()
    if t in ("0", "0.0"):
        return 0, 0
    m = re.match(r"vec2\(\s*(" + FLOAT + r")\s*,\s*(" + FLOAT + r")\s*\)$", t)
    assert m, text
    return int(float(m.group(1))), int(float(m.group(2)))


def parse_pass(block):
    p = Pass(block["desc"], block["binds"], block["save"])
    code = block["code"]
    # Statements of the hook() body.
    body = code[code.index("{") + 1: code.rindex("}")]
    stmts = [s.strip() for s in body.split(";") if s.strip()]
    acc = None
    for s in stmts:
        m = re.match(r"vec4 (\w+) = vec4\((.*)\)$", s)
        if m:
            acc = m.group(1)
            p.bias = floats(m.group(2))
            assert len(p.bias) == 4
            continue
        assert acc, s
        # luma tap: acc += vec4(w) * float(LUMA_texOff(o))  |  * LUMA_texOff(o).x
        m = re.match(acc + r" \+= vec4\(([^)]*)\) \* (?:float\()?LUMA_texOff\((.*?\)|0)\)(?:\.x|\))$", s)
        if m:
            w = floats(m.group(1))
            dx, dy = parse_offset(m.group(2))
            p.terms.append(Term([[w[i]] for i in range(4)], [("LUMA", dx, dy)]))
            continue
        # matrix tap over one input, or over the sum of two (ArtCNN's long skip)
        m = re.match(acc + r" \+= mat4(?:x4)?\(([^)]*)\) \* (.*)$", s)
        if m and "_texOff" in m.group(2):
            w = mat4_rows(floats(m.group(1)))
            srcs = []
            for tm in re.finditer(r"(\w+)_texOff\(((?:vec2\([^)]*\))|0)\)", m.group(2)):
                dx, dy = parse_offset(tm.group(2))
                srcs.append((tm.group(1), dx, dy))
            assert srcs, s
            p.terms.append(Term(w, srcs))
            continue
        # identity add (FSRCNNX's sub-band residual)
        m = re.match(acc + r" \+= (\w+)_texOff\(((?:vec2\([^)]*\))|0)\)$", s)
        if m:
            dx, dy = parse_offset(m.group(2))
            p.terms.append(Term([[1.0 if i == j else 0.0 for j in range(4)] for i in range(4)],
                                [(m.group(1), dx, dy)]))
            continue
        # PReLU
        m = re.match(acc + r" = max\(" + acc + r", vec4\(0\.0\)\) \+ vec4\(([^)]*)\) \* min\(" + acc +
                     r", vec4\(0\.0\)\)$", s)
        if m:
            p.act = ("prelu", floats(m.group(1)))
            continue
        if s in ("return " + acc, "return vec4(" + acc + ")"):
            continue
        if s == "return max(" + acc + ", vec4(0.0))":
            p.act = ("relu",)
            continue
        raise SystemExit("gen_cnn_pipes: unknown statement in %r: %s" % (p.name, s[:120]))
    return p


def parse_depth_to_space(block):
    """The final 2x pass: which channel holds which sub-pixel, and a clamp."""
    code = block["code"]
    if re.search(r"\[\s*index\.x \* 2 \+ index\.y\s*\]", code):
        order = "x_major"      # FSRCNNX: channel = 2 x + y
    elif re.search(r"\[\s*i0\.y \* 2 \+ i0\.x\s*\]", code):
        order = "y_major"      # ArtCNN: channel = 2 y + x (Emby5's own layout)
    else:
        raise SystemExit("gen_cnn_pipes: depth-to-space layout not recognised in %r" % block["desc"])
    return {"order": order, "clamp": "clamp(result, 0.0, 1.0)" in code}


def load_network(path):
    blocks = hook_blocks(path)
    if any(b["compute"] for b in blocks):
        raise SystemExit("gen_cnn_pipes: %s uses compute passes; use the fragment variant" % path)
    passes = []
    final = None
    for b in blocks:
        if b["save"] is None:
            final = parse_depth_to_space(b)
            continue
        passes.append(parse_pass(b))
    assert final, path
    passes[-1].final = final
    return passes


# --------------------------------------------------------------------------
# Surfaces: SSA versions of the saved textures -> pool slots
# --------------------------------------------------------------------------

def plan_surfaces(passes):
    """Give every pass its inputs (descriptor order) and an output slot.

    A texture lives from the pass that writes it to the last pass that reads
    it; a pass never writes a slot it reads. Slot 0 is L0 (the source)."""
    version = {}            # texture name -> producing pass index
    reads = []
    for i, p in enumerate(passes):
        used = []
        for t in p.terms:
            for name, _, _ in t.srcs:
                key = "LUMA" if name == "LUMA" else version[name]
                if key not in used:
                    used.append(key)
        if p.final is not None and "LUMA" not in used:
            used.append("LUMA")    # the bilinear source luma of the residual
        reads.append(used)
        version[p.save] = i
    last_use = {}
    for i, used in enumerate(reads):
        for k in used:
            if k != "LUMA":
                last_use[k] = i
    slot_of, free, busy = {}, list(range(1, POOL_SLOTS + 1)), {}
    plan = []
    for i, p in enumerate(passes):
        for k in [k for k in busy if last_use.get(k, -1) < i]:
            free.append(busy.pop(k))
        free.sort()
        if not free:
            raise SystemExit("gen_cnn_pipes: more than %d live maps" % POOL_SLOTS)
        out = free.pop(0)
        busy[i] = out
        slot_of[i] = out
        inputs = [0 if k == "LUMA" else slot_of[k] for k in reads[i]]
        plan.append({"in": inputs, "keys": reads[i], "out": out})
    return plan, max(slot_of.values())


# --------------------------------------------------------------------------
# One pass as straight-line scalar code, printed as LLVM IR below
# --------------------------------------------------------------------------

class Code:
    def __init__(self):
        self.ops = []   # (dst, op, args)
        self.n = 0
        self.fetched = {}

    def new(self, op, *args):
        self.n += 1
        dst = "%%v%d" % self.n
        self.ops.append((dst, op, args))
        return dst


def build_pass_code(p, plan_entry):
    """Ops: fetch(tex, dx, dy) -> 4 values; luma(r, g, b); fma(w, a, c); ..."""
    c = Code()
    keys = plan_entry["keys"]

    def fetch(desc, dx, dy):
        k = (desc, dx, dy)
        if k not in c.fetched:
            v = c.new("fetch", desc, dx, dy)
            c.fetched[k] = [c.new("extract", v, j) for j in range(4)]
        return c.fetched[k]

    def luma(dx, dy):
        k = ("luma", dx, dy)
        if k not in c.fetched:
            rgb = fetch(keys.index("LUMA"), dx, dy)
            y = c.new("fmul", rgb[0], LUMA[0])
            y = c.new("fma", rgb[1], LUMA[1], y)
            y = c.new("fma", rgb[2], LUMA[2], y)
            c.fetched[k] = y
        return c.fetched[k]

    # The descriptor slot of each source name: the plan lists the producing
    # pass of every texture this pass reads, in first-use order.
    p_inputs = plan_entry["src_desc"]

    acc = [c.new("const", b) for b in p.bias]
    for t in p.terms:
        if t.srcs[0][0] == "LUMA":
            (_, dx, dy), = t.srcs
            y = luma(dx, dy)
            acc = [c.new("fma", y, t.w[i][0], acc[i]) if t.w[i][0] != 0.0 else acc[i] for i in range(4)]
            continue
        vals = None
        for name, dx, dy in t.srcs:
            v = fetch(p_inputs[name], dx, dy)
            vals = v if vals is None else [c.new("fadd", vals[j], v[j]) for j in range(4)]
        ident = all(t.w[i][j] == (1.0 if i == j else 0.0) for i in range(4) for j in range(4))
        for i in range(4):
            if ident:
                acc[i] = c.new("fadd", acc[i], vals[i])
                continue
            for j in range(4):
                if t.w[i][j] != 0.0:
                    acc[i] = c.new("fma", vals[j], t.w[i][j], acc[i])
    if p.act and p.act[0] == "relu":
        acc = [c.new("max0", a) for a in acc]
    elif p.act and p.act[0] == "prelu":
        acc = [c.new("fma", c.new("min0", a), p.act[1][i], c.new("max0", a)) for i, a in enumerate(acc)]
    if p.final is not None:
        if p.final["order"] == "x_major":     # channel 2 x + y -> Emby5's 2 y + x
            acc = [acc[0], acc[2], acc[1], acc[3]]
        if p.final["clamp"]:
            acc = [c.new("clamp01", a) for a in acc]
        l0 = keys.index("LUMA")
        out = []
        for ch in range(4):
            sx, sy = ch & 1, ch >> 1
            s = c.new("sample", l0, sx, sy)
            rgb = [c.new("extract", s, j) for j in range(3)]
            y = c.new("fmul", rgb[0], LUMA[0])
            y = c.new("fma", rgb[1], LUMA[1], y)
            y = c.new("fma", rgb[2], LUMA[2], y)
            out.append(c.new("fsub", acc[ch], y))
        acc = out
    c.out = acc
    return c


def attach_descriptors(passes, plan):
    """Map each source texture NAME of a pass to its descriptor index."""
    version = {}
    for i, (p, e) in enumerate(zip(passes, plan)):
        src_desc = {}
        for t in p.terms:
            for name, _, _ in t.srcs:
                if name != "LUMA":
                    src_desc[name] = e["keys"].index(version[name])
        e["src_desc"] = src_desc
        version[p.save] = i


# --------------------------------------------------------------------------
# LLVM IR
# --------------------------------------------------------------------------

def fconst(x):
    """An LLVM IR float literal: the float32 value as a hex double."""
    f32 = struct.unpack("<f", struct.pack("<f", float(x)))[0]
    return "0x%016X" % struct.unpack("<Q", struct.pack("<d", f32))[0]


IR_HEAD = """target triple = "amdgcn--amdpal"

declare i64 @llvm.amdgcn.s.getpc()
declare float @llvm.amdgcn.interp.p1(float, i32, i32, i32)
declare float @llvm.amdgcn.interp.p2(float, float, i32, i32, i32)
declare <4 x float> @llvm.amdgcn.image.load.2d.v4f32.i32(i32, i32, i32, <8 x i32>, i32, i32)
declare <4 x float> @llvm.amdgcn.image.sample.lz.2d.v4f32.f32(i32, float, float, <8 x i32>, <4 x i32>, i1, i32, i32)
declare <2 x half> @llvm.amdgcn.cvt.pkrtz(float, float)
declare void @llvm.amdgcn.exp.compr.v2f16(i32, i32, <2 x half>, <2 x half>, i1, i1)
declare float @llvm.fma.f32(float, float, float)
declare float @llvm.maxnum.f32(float, float)
declare float @llvm.minnum.f32(float, float)
declare i32 @llvm.smax.i32(i32, i32)
declare i32 @llvm.smin.i32(i32, i32)

; s0 global table (unused), s1 descriptor table, s2 prim mask; v0/v1 = PERSP_CENTER.
define dllexport amdgpu_ps void @_amdgpu_ps_main(i32 inreg %gt, i32 inreg %tab, i32 inreg %prim, <2 x float> %persp_sample, <2 x float> %persp_center) #0 {
entry:
  %pc = call i64 @llvm.amdgcn.s.getpc()
  %pchi = and i64 %pc, -4294967296
  %tablo = zext i32 %tab to i64
  %tabaddr = or i64 %pchi, %tablo
  %tp = inttoptr i64 %tabaddr to ptr addrspace(4)
  %bi = extractelement <2 x float> %persp_center, i32 0
  %bj = extractelement <2 x float> %persp_center, i32 1
  %u1 = call float @llvm.amdgcn.interp.p1(float %bi, i32 0, i32 0, i32 %prim)
  %u = call float @llvm.amdgcn.interp.p2(float %u1, float %bj, i32 0, i32 0, i32 %prim)
  %w1 = call float @llvm.amdgcn.interp.p1(float %bi, i32 1, i32 0, i32 %prim)
  %w = call float @llvm.amdgcn.interp.p2(float %w1, float %bj, i32 1, i32 0, i32 %prim)
"""

IR_TAIL = """  ret void
}

attributes #0 = { nounwind WAVES"target-features"="+wavefrontsize64" "denormal-fp-math-f32"="preserve-sign,preserve-sign" }
!0 = !{}
"""


def emit_ir(code, n_tex, uses_sample, clause=True):
    L = [IR_HEAD]
    for k in range(n_tex):
        L.append("  %%tp%d = getelementptr i8, ptr addrspace(4) %%tp, i64 %d\n" % (k, 48 * k))
        L.append("  %%t%d = load <8 x i32>, ptr addrspace(4) %%tp%d, align 16, !invariant.load !0\n" % (k, k))
    if uses_sample is not None:
        L.append("  %%sp%d = getelementptr i8, ptr addrspace(4) %%tp, i64 %d\n" % (uses_sample, 48 * uses_sample + 32))
        L.append("  %%s%d = load <4 x i32>, ptr addrspace(4) %%sp%d, align 16, !invariant.load !0\n" % (uses_sample, uses_sample))
    # Size of the maps (every texture of a pass has the source's size): the
    # T#'s WIDTH-1 is bits 62..75, HEIGHT-1 bits 78..91 (LLPC's textureSize()).
    L.append("""  %d1 = extractelement <8 x i32> %t0, i32 1
  %d2 = extractelement <8 x i32> %t0, i32 2
  %wa = lshr i32 %d1, 30
  %wb = shl i32 %d2, 2
  %wc = or i32 %wa, %wb
  %wm = and i32 %wc, 16383
  %hs = lshr i32 %d2, 14
  %hm = and i32 %hs, 16383
  %wd = add i32 %wm, 1
  %hd = add i32 %hm, 1
  %wf = uitofp i32 %wd to float
  %hf = uitofp i32 %hd to float
  %xf = fmul float %u, %wf
  %yf = fmul float %w, %hf
  %xi = fptosi float %xf to i32
  %yi = fptosi float %yf to i32
""")
    offs_x = sorted({a[1] for _, op, a in code.ops if op == "fetch"})
    offs_y = sorted({a[2] for _, op, a in code.ops if op == "fetch"})

    def coord(axis, o):
        return "%%%s%s%d" % (axis, "m" if o < 0 else "p", abs(o))

    for axis, offs, lim in (("x", offs_x, "%wm"), ("y", offs_y, "%hm")):
        for o in offs:
            name = coord(axis, o)
            L.append("  %s.a = add i32 %%%si, %d\n" % (name, axis, o))
            L.append("  %s.b = call i32 @llvm.smax.i32(i32 %s.a, i32 0)\n" % (name, name))
            L.append("  %s = call i32 @llvm.smin.i32(i32 %s.b, i32 %s)\n" % (name, name, lim))
    if uses_sample is not None:
        L.append("  %rw = fdiv afn float 1.0, %wf\n  %rh = fdiv afn float 1.0, %hf\n")
        L.append("  %xs = sitofp i32 %xi to float\n  %ys = sitofp i32 %yi to float\n")
    # Every fetch first: with the occupancy pinned at 8 waves (64 VGPRs) the
    # scheduler then issues them together, the way amdllpc's Anime4K passes
    # do, instead of a load and a wait per tap. A pass with too many taps for
    # that (it would spill) keeps llc's own order and occupancy.
    ops = code.ops
    if clause:
        first = [o for o in ops if o[1] in ("fetch", "sample", "extract")]
        ops = first + [o for o in ops if o[1] not in ("fetch", "sample", "extract")]
    for dst, op, a in ops:
        if op == "fetch":
            L.append("  %s = call <4 x float> @llvm.amdgcn.image.load.2d.v4f32.i32(i32 15, i32 %s, i32 %s, "
                     "<8 x i32> %%t%d, i32 0, i32 0)\n" % (dst, coord("x", a[1]), coord("y", a[2]), a[0]))
        elif op == "extract":
            L.append("  %s = extractelement <4 x float> %s, i32 %d\n" % (dst, a[0], a[1]))
        elif op == "const":
            L.append("  %s = fadd float %s, 0.0\n" % (dst, fconst(a[0])))
        elif op == "fmul":
            L.append("  %s = fmul float %s, %s\n" % (dst, a[0], fconst(a[1])))
        elif op == "fma":
            L.append("  %s = call float @llvm.fma.f32(float %s, float %s, float %s)\n" % (dst, a[0], fconst(a[1]), a[2]))
        elif op == "fadd":
            L.append("  %s = fadd float %s, %s\n" % (dst, a[0], a[1]))
        elif op == "fsub":
            L.append("  %s = fsub float %s, %s\n" % (dst, a[0], a[1]))
        elif op == "max0":
            L.append("  %s = call float @llvm.maxnum.f32(float %s, float 0.0)\n" % (dst, a[0]))
        elif op == "min0":
            L.append("  %s = call float @llvm.minnum.f32(float %s, float 0.0)\n" % (dst, a[0]))
        elif op == "clamp01":
            L.append("  %s.c = call float @llvm.maxnum.f32(float %s, float 0.0)\n" % (dst, a[0]))
            L.append("  %s = call float @llvm.minnum.f32(float %s.c, float 1.0)\n" % (dst, dst))
        elif op == "sample":
            # bilinear source at the sub-pixel centre (x + 0.25 + 0.5 sx, y + ...)
            fx = fconst(0.25 + 0.5 * a[1])
            fy = fconst(0.25 + 0.5 * a[2])
            L.append("  %s.x = fadd float %%xs, %s\n  %s.u = fmul float %s.x, %%rw\n" % (dst, fx, dst, dst))
            L.append("  %s.y = fadd float %%ys, %s\n  %s.v = fmul float %s.y, %%rh\n" % (dst, fy, dst, dst))
            L.append("  %s = call <4 x float> @llvm.amdgcn.image.sample.lz.2d.v4f32.f32(i32 7, float %s.u, "
                     "float %s.v, <8 x i32> %%t%d, <4 x i32> %%s%d, i1 false, i32 0, i32 0)\n"
                     % (dst, dst, dst, a[0], a[0]))
        else:
            raise AssertionError(op)
    o = code.out
    L.append("  %%lo = call <2 x half> @llvm.amdgcn.cvt.pkrtz(float %s, float %s)\n" % (o[0], o[1]))
    L.append("  %%hi = call <2 x half> @llvm.amdgcn.cvt.pkrtz(float %s, float %s)\n" % (o[2], o[3]))
    L.append("  call void @llvm.amdgcn.exp.compr.v2f16(i32 0, i32 15, <2 x half> %lo, <2 x half> %hi, i1 true, i1 true)\n")
    L.append(IR_TAIL.replace("WAVES", '"amdgpu-waves-per-eu"="8,8" ' if clause else ""))
    return "".join(L)


def compile_ir(llc, ir, work, name):
    ll = os.path.join(work, name + ".ll")
    with open(ll, "w") as f:
        f.write(ir)
    asm = subprocess.run([llc, "-mtriple=amdgcn--amdpal", "-mcpu=gfx1013", "-O3", ll, "-o", "-"],
                         check=True, capture_output=True, text=True).stdout
    obj = os.path.join(work, name + ".o")
    subprocess.run([llc, "-mtriple=amdgcn--amdpal", "-mcpu=gfx1013", "-O3", "-filetype=obj", ll, "-o", obj],
                   check=True)
    objcopy = shutil.which(llc.replace("llc", "llvm-objcopy")) or shutil.which("llvm-objcopy")
    readobj = shutil.which(llc.replace("llc", "llvm-readobj")) or shutil.which("llvm-readobj")
    rel = subprocess.run([readobj, "-r", obj], check=True, capture_output=True, text=True).stdout
    if re.search(r"R_AMDGPU", rel):
        raise SystemExit("gen_cnn_pipes: %s has relocations" % name)
    binf = os.path.join(work, name + ".bin")
    subprocess.run([objcopy, "-O", "binary", "--only-section=.text", obj, binf], check=True)
    with open(binf, "rb") as f:
        isa = f.read()
    # Drop the s_code_end padding llc appends to the section.
    while len(isa) >= 4 and isa[-4:] == b"\x00\x00\x9f\xbf":
        isa = isa[:-4]
    meta = {}
    for key in ("scratch_memory_size", "sgpr_count", "vgpr_count"):
        m = re.search(r"\." + key + r":\s+(0x[0-9a-f]+|\d+)", asm)
        meta[key] = int(m.group(1), 0)
    for reg in ("0x2c0a", "0xa1b3", "0xa1b4"):
        m = re.search(reg + r" \([A-Z0-9_]+\):\s+(0x[0-9a-f]+|\d+)", asm)
        meta[reg] = int(m.group(1), 0)
    if meta["scratch_memory_size"]:
        return None, meta
    if meta["0xa1b3"] != 2 or meta["0xa1b4"] != 2:
        raise SystemExit("gen_cnn_pipes: %s: PS inputs %#x/%#x, expected PERSP_CENTER only"
                         % (name, meta["0xa1b3"], meta["0xa1b4"]))
    if (meta["0x2c0a"] & ~0x3ff) != 0x022c0000:
        raise SystemExit("gen_cnn_pipes: %s: RSRC1 %#x has unexpected mode bits" % (name, meta["0x2c0a"]))
    return isa, meta


# --------------------------------------------------------------------------
# Headers
# --------------------------------------------------------------------------

def read_template():
    src = open(TEMPLATE, encoding="utf-8").read()

    def block(name):
        m = re.search(r"upscale_a4k_final_" + name + r"\[[^\]]*\][^=]*= \{\n(.*?)\n\};", src, re.S)
        return m.group(1)

    def define(name):
        return re.search(r"#define UPSCALE_A4K_FINAL_" + name + r" (\S+)", src).group(1)

    t = {"gs_isa": block("gs_isa"), "pre_raster_cx": block("pre_raster_cx"),
         "pixel_cx": block("pixel_cx"), "ps_input_cntl": block("ps_input_cntl")}
    for d in ("GS_ISA_BYTES", "GS_RSRC1", "GS_RSRC2", "PS_RSRC1", "PS_RSRC2", "GE_CNTL",
              "SHADER_STAGES_EN", "GS_OUT_PRIM_TYPE", "DRAW_MODIFIER", "GS_USER_SGPRS",
              "PS_USER_SGPRS", "VS_CONST_TABLE_DWORD", "VS_VERTEX_TABLE_DWORD",
              "PS_CONST_TABLE_DWORD", "PS_TEXTURE_TABLE_DWORD", "VS_WRITE_COUNT", "PS_WRITE_COUNT"):
        t[d] = define(d)
    # The user-data fields of the metadata exactly as the template has them
    # (its .vs_user_sgpr_count is not its GS_USER_SGPRS define).
    meta = re.search(r"upscale_a4k_final_metadata = \{(.*?)\};", src, re.S).group(1)
    for f in ("vs_user_sgpr_count", "ps_user_sgpr_count", "vs_const_table_dword",
              "vs_vertex_table_dword", "ps_const_table_dword", "ps_texture_table_dword"):
        t["meta_" + f] = re.search(r"\." + f + r" = ([^,]+),", meta).group(1)
    t["user_data_comment"] = re.search(r"/\* PAL user_data_reg_map.*?\*/", src, re.S).group(0)
    for key in ("pre_raster_cx", "pixel_cx", "ps_input_cntl"):
        t[key + "_count"] = len(re.findall(r"\{0x", t[key]))
    if int(t["PS_RSRC1"].rstrip("u"), 16) & ~0x3ff != 0x022c0000:
        raise SystemExit("gen_cnn_pipes: the template's RSRC1 mode bits changed")
    return t


def hex_rows(data):
    rows = []
    for i in range(0, len(data), 16):
        rows.append("    " + " ".join("0x%02x," % b for b in data[i:i + 16]))
    return "\n".join(rows)


def write_pipe_header(t, name, isa, rsrc1, what):
    up = name.upper()
    ps_rsrc1 = (int(t["PS_RSRC1"].rstrip("u"), 16) & ~0x3ff) | (rsrc1 & 0x3ff)
    h = []
    h.append("/* %s - generated by app/scripts/gen_cnn_pipes.py from %s. DO NOT EDIT.\n" % (name, what))
    h.append(" * Pixel stage: LLVM IR compiled with llc -mcpu=gfx1013 (wave64, PAL ABI); the\n"
             " * vertex stage and every register but the pixel stage's VGPR/SGPR counts are\n"
             " * upscale_a4k_final's, which all upscale pipes share. */\n")
    h.append("#ifndef EVO_AGC_PIPE_%s_H\n#define EVO_AGC_PIPE_%s_H\n\n" % (up, up))
    h.append('#include "evo_agc_shader_header.h"\n\n')
    h.append("#define %s_GS_ISA_BYTES %s\n" % (up, t["GS_ISA_BYTES"]))
    h.append("#define %s_PS_ISA_BYTES %du\n" % (up, len(isa)))
    for d in ("GS_RSRC1", "GS_RSRC2"):
        h.append("#define %s_%s %s\n" % (up, d, t[d]))
    h.append("#define %s_PS_RSRC1 0x%08xu\n" % (up, ps_rsrc1))
    for d in ("PS_RSRC2", "GE_CNTL", "SHADER_STAGES_EN", "GS_OUT_PRIM_TYPE", "DRAW_MODIFIER",
              "GS_USER_SGPRS", "PS_USER_SGPRS", "VS_CONST_TABLE_DWORD", "VS_VERTEX_TABLE_DWORD",
              "PS_CONST_TABLE_DWORD", "PS_TEXTURE_TABLE_DWORD", "VS_WRITE_COUNT", "PS_WRITE_COUNT"):
        h.append("#define %s_%s %s\n" % (up, d, t[d]))
    h.append("\n" + t["user_data_comment"] + "\n\n")
    h.append("static const uint8_t %s_gs_isa[%d] __attribute__((aligned(256))) = {\n%s\n};\n\n"
             % (name, int(t["GS_ISA_BYTES"].rstrip("u")), t["gs_isa"]))
    h.append("static const uint8_t %s_ps_isa[%d] __attribute__((aligned(256))) = {\n%s\n};\n\n"
             % (name, len(isa), hex_rows(isa)))
    for key in ("pre_raster_cx", "pixel_cx", "ps_input_cntl"):
        h.append("static const evo_agc_reg_t %s_%s[] = {\n%s\n};\n\n" % (name, key, t[key]))
    h.append("static const evo_agc_shader_metadata_t %s_metadata = {\n" % name)
    h.append("    .gs_isa = %s_gs_isa,\n    .gs_isa_bytes = %s,\n" % (name, t["GS_ISA_BYTES"]))
    h.append("    .ps_isa = %s_ps_isa,\n    .ps_isa_bytes = %du,\n" % (name, len(isa)))
    for f, d in (("gs_rsrc1", "GS_RSRC1"), ("gs_rsrc2", "GS_RSRC2"), ("ps_rsrc1", "PS_RSRC1"),
                 ("ps_rsrc2", "PS_RSRC2"), ("ge_cntl", "GE_CNTL"), ("shader_stages_en", "SHADER_STAGES_EN"),
                 ("gs_out_prim_type", "GS_OUT_PRIM_TYPE"), ("draw_modifier", "DRAW_MODIFIER")):
        h.append("    .%s = %s_%s,\n" % (f, up, d))
    for key in ("pre_raster_cx", "pixel_cx", "ps_input_cntl"):
        h.append("    .%s = %s_%s,\n    .%s_count = %du,\n" % (key, name, key, key, t[key + "_count"]))
    for f in ("vs_user_sgpr_count", "ps_user_sgpr_count", "vs_const_table_dword",
              "vs_vertex_table_dword", "ps_const_table_dword", "ps_texture_table_dword"):
        h.append("    .%s = %s,\n" % (f, t["meta_" + f]))
    h.append("};\n\n#endif /* EVO_AGC_PIPE_%s_H */\n" % up)
    with open(os.path.join(OUT_DIR, name + "_pipe.h"), "w", encoding="utf-8") as f:
        f.write("".join(h))


def cost(passes, plan):
    macs = sum(sum(sum(1 for x in row if x != 0.0) for row in t.w) * 1 for p in passes for t in p.terms)
    fetches = 0
    for p, e in zip(passes, plan):
        keys = set()
        for t in p.terms:
            for name, dx, dy in t.srcs:
                keys.add((name, dx, dy))
        fetches += len(keys) + (4 if p.final else 0)
    return macs, fetches


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--llc", default=shutil.which("llc-18") or shutil.which("llc") or "llc")
    ap.add_argument("--check", action="store_true", help="parse and plan only")
    args = ap.parse_args()

    nets = []
    for prefix, src, pipe_base, what in NETWORKS:
        passes = load_network(os.path.join(SRC_DIR, src))
        plan, slots = plan_surfaces(passes)
        attach_descriptors(passes, plan)
        macs, fetches = cost(passes, plan)
        print("%s: %d passes, %d maps, %d MAC and %d texel fetches per source pixel"
              % (prefix, len(passes), slots, macs, fetches))
        for i, (p, e) in enumerate(zip(passes, plan)):
            print("  p%-2d %-40s in=%s out=%d" % (i, p.name, e["in"], e["out"]))
        nets.append((prefix, src, pipe_base, what, passes, plan, slots, macs, fetches))
    if args.check:
        return 0

    t = read_template()
    work = tempfile.mkdtemp(prefix="gen_cnn_pipes.")
    inc, hdr = [], []
    try:
        for prefix, src, pipe_base, what, passes, plan, slots, macs, fetches in nets:
            for i, (p, e) in enumerate(zip(passes, plan)):
                code = build_pass_code(p, e)
                name = "upscale_%s_p%d" % (prefix, i)
                for clause in (True, False):
                    ir = emit_ir(code, len(e["in"]), e["keys"].index("LUMA") if p.final else None, clause)
                    isa, meta = compile_ir(args.llc, ir, work, name)
                    if isa is not None:
                        break
                if isa is None:
                    raise SystemExit("gen_cnn_pipes: %s spills to scratch" % name)
                write_pipe_header(t, name, isa, meta["0x2c0a"],
                                  "%s, pass %d (%s)" % (src, i, p.name))
                print("  %s: %d bytes, %d VGPRs" % (name, len(isa), meta["vgpr_count"]))
                inc.append('{%s + %d, &%s_metadata, "%s"},\n' % (pipe_base, i, name, name))
                hdr.append('#include "%s_pipe.h"\n' % name)
    finally:
        shutil.rmtree(work, ignore_errors=True)

    with open(os.path.join(OUT_DIR, "upscale_cnn_pipes.inc"), "w", encoding="utf-8") as f:
        f.write("/* Generated by app/scripts/gen_cnn_pipes.py - DO NOT EDIT. Rows of the\n"
                " * runtime's pipeline table for the FSRCNNX and ArtCNN passes. */\n")
        f.write("".join(inc))
    with open(os.path.join(OUT_DIR, "upscale_cnn_pipes.h"), "w", encoding="utf-8") as f:
        f.write("/* Generated by app/scripts/gen_cnn_pipes.py - DO NOT EDIT.\n"
                " * The FSRCNNX (\"Film\") and ArtCNN (\"Anime\") passes and the scratch\n"
                " * surfaces each reads and writes: slot 0 is L0 (the source, RGBA8), 1..%d\n"
                " * the RGBA16F maps of the upscaler's pool. The last pass leaves the 2x luma\n"
                " * residual (depth-to-space) for upscale_a4k_final. */\n" % POOL_SLOTS)
        f.write("#ifndef EVO_AGC_UPSCALE_CNN_PIPES_H\n#define EVO_AGC_UPSCALE_CNN_PIPES_H\n\n")
        f.write("".join(hdr))
        f.write("\ntypedef struct {\n    uint8_t out_slot;\n    uint8_t n_in;\n    uint8_t in_slot[6];\n"
                "} evo_agc_up_cnn_pass_t;\n\n")
        for prefix, src, pipe_base, what, passes, plan, slots, macs, fetches in nets:
            up = prefix.upper()
            f.write("/* %s: %d MAC and %d texel fetches per source pixel. */\n" % (what, macs, fetches))
            f.write("#define EVO_AGC_UP_%s_PASSES %d\n#define EVO_AGC_UP_%s_SLOTS %d\n" % (up, len(passes), up, slots))
            f.write("static const evo_agc_up_cnn_pass_t k_up_%s_passes[%d] = {\n" % (prefix, len(passes)))
            for i, (p, e) in enumerate(zip(passes, plan)):
                ins = ", ".join(str(s) for s in e["in"])
                f.write("    {%d, %d, {%s}},   /* %s */\n" % (e["out"], len(e["in"]), ins, p.name))
            f.write("};\n\n")
        f.write("#endif /* EVO_AGC_UPSCALE_CNN_PIPES_H */\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
