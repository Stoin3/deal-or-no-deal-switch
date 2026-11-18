"""Builds the PC test version with zig (clang) + SDL2."""
import glob, os, shutil, subprocess, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SDL = os.path.join(ROOT, "tools", "deps", "SDL2-2.32.10", "x86_64-w64-mingw32")
OUT = os.path.join(ROOT, "build_pc")
os.makedirs(OUT, exist_ok=True)
objs = []
for src in glob.glob(os.path.join(ROOT, "source", "*.cpp")):
    if src.endswith("platform_switch.cpp"):
        continue
    obj = os.path.join(OUT, os.path.basename(src) + ".o")
    if not os.path.exists(obj) or os.path.getmtime(obj) < max(os.path.getmtime(src), *[os.path.getmtime(h) for h in glob.glob(os.path.join(ROOT, "source", "*.h"))]):
        cmd = [sys.executable, "-m", "ziglang", "c++", "-target", "x86_64-windows-gnu", "-std=c++17", "-O2", "-g0",
               "-Wno-unused-function", "-I" + os.path.join(SDL, "include", "SDL2"), "-c", src, "-o", obj]
        print("cc", os.path.basename(src)); sys.stdout.flush()
        r = subprocess.run(cmd)
        if r.returncode: sys.exit(1)
    objs.append(obj)
exe = os.path.join(OUT, "kofferkoorts.exe")
cmd = [sys.executable, "-m", "ziglang", "c++", "-target", "x86_64-windows-gnu", *objs, os.path.join(SDL, "lib", "libSDL2.dll.a"), "-o", exe]
r = subprocess.run(cmd)
if r.returncode: sys.exit(1)
shutil.copy(os.path.join(SDL, "bin", "SDL2.dll"), OUT)
print("built", exe)
