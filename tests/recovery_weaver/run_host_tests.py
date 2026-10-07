"""Compile production Weaver1.cpp against fake Binder transports, then run it.
This tests client control flow; it is not an Android Binder integration test.
Usage: python tests/recovery_weaver/run_host_tests.py --cxx clang++
For Zig: --cxx /path/to/zig.exe --zig
"""
import argparse, pathlib, subprocess, tempfile, os
p=argparse.ArgumentParser();p.add_argument('--cxx',default='c++');p.add_argument('--zig',action='store_true');p.add_argument('--crypto',action='store_true');a=p.parse_args()
here=pathlib.Path(__file__).resolve().parent;root=here.parents[1]
with tempfile.TemporaryDirectory(prefix='weaver-test-') as tmp:
    out=pathlib.Path(tmp)
    for path in ['android/binder_manager.h','android-base/logging.h','android-base/macros.h','android/hardware/weaver/1.0/IWeaver.h','aidl/android/hardware/weaver/IWeaver.h']:
        h=out/path;h.parent.mkdir(parents=True,exist_ok=True);h.write_text('#include "fake_android.h"\n')
    compiler=[a.cxx]+(['c++'] if a.zig else [])
    cases=[('format',['format_test.cpp']),('client',['client_test.cpp',str(root/'Weaver1.cpp')])]
    if a.crypto: cases.append(('crypto',['crypto_test.cpp']))
    for name,files in cases:
        exe=out/(name+('.exe' if os.name=='nt' else ''))
        cmd=compiler+['-std=c++17','-Wall','-Wextra','-Werror','-I'+str(out),'-I'+str(here),'-I'+str(root)]+[str(here/f) for f in files]+['-o',str(exe)]
        if name=='crypto': cmd+=['-lcrypto']
        subprocess.run(cmd,check=True);subprocess.run([str(exe)],check=True,timeout=30)
