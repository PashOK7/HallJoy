"""Exercise the real Windows job, child inheritance, quoting and exit status."""
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

TOOLS=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(TOOLS))
import build_cpu_budget as budget

PROBE=r'''
import ctypes,json,sys,subprocess
from ctypes import wintypes as w
from build_cpu_budget import apply_current
apply_current() # Rejoining must not create a second90% quota.
k=ctypes.WinDLL('kernel32',use_last_error=True)
k.GetCurrentProcess.restype=w.HANDLE
k.GetPriorityClass.argtypes=[w.HANDLE];k.GetPriorityClass.restype=w.DWORD
k.OpenJobObjectW.argtypes=[w.DWORD,w.BOOL,w.LPCWSTR];k.OpenJobObjectW.restype=w.HANDLE
k.IsProcessInJob.argtypes=[w.HANDLE,w.HANDLE,ctypes.POINTER(w.BOOL)]
k.QueryInformationJobObject.argtypes=[w.HANDLE,ctypes.c_int,ctypes.c_void_p,w.DWORD,ctypes.c_void_p]
class Cpu(ctypes.Structure):_fields_=[('flags',w.DWORD),('rate',w.DWORD)]
job=k.OpenJobObjectW(4,False,'Local\\HallJoy.BuildCpuBudget90.v1'.replace('\\\\','\\'))
assert job,ctypes.get_last_error()
cpu=Cpu();assert k.QueryInformationJobObject(job,15,ctypes.byref(cpu),ctypes.sizeof(cpu),None)
member=w.BOOL();assert k.IsProcessInJob(k.GetCurrentProcess(),job,ctypes.byref(member))
assert member.value and cpu.flags==5 and cpu.rate==9000
assert k.GetPriorityClass(k.GetCurrentProcess())==0x4000
if sys.argv[1]=='parent':
    child=subprocess.run([sys.executable,'-c',sys.argv[2],'child',*sys.argv[3:]],capture_output=True,text=True,check=True)
    print(child.stdout,end='')
else:print(json.dumps({'flags':cpu.flags,'rate':cpu.rate,'priority':0x4000,'arguments':sys.argv[2:]}))
'''


class BuildCpuBudgetTests(unittest.TestCase):
    @unittest.skipUnless(os.name=='nt','Windows kernel job test')
    def test_real_budget_descendants_arguments_and_exit_code(self):
        k=ctypes.WinDLL('kernel32',use_last_error=True)
        k.GetCurrentProcess.restype=wintypes.HANDLE
        k.GetPriorityClass.argtypes=[wintypes.HANDLE];k.GetPriorityClass.restype=wintypes.DWORD
        original=k.GetPriorityClass(k.GetCurrentProcess())
        args=['two words','quote"here','slash\\','']
        env=dict(os.environ,PYTHONPATH=str(TOOLS))
        result=subprocess.run([sys.executable,str(TOOLS/'build_cpu_budget.py'),'--run',
            sys.executable,'-c',PROBE,'parent',PROBE,*args],capture_output=True,text=True,env=env,timeout=30)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        payload=json.loads(result.stdout.splitlines()[-1])
        self.assertEqual(payload,dict(flags=5,rate=9000,priority=0x4000,arguments=args))
        self.assertEqual(k.GetPriorityClass(k.GetCurrentProcess()),original)
        result=subprocess.run([sys.executable,str(TOOLS/'build_cpu_budget.py'),'--run',
            sys.executable,'-c','raise SystemExit(17)'],capture_output=True,text=True,timeout=20)
        self.assertEqual(result.returncode,17,result.stdout+result.stderr)

    def test_failed_budget_never_starts_command(self):
        with patch.object(sys,'argv',['budget','--run','compiler']), \
             patch.object(budget,'apply_current',side_effect=OSError('budget unavailable')), \
             patch.object(budget.subprocess,'run') as run:
            with self.assertRaises(OSError):budget.main()
            run.assert_not_called()


if __name__=='__main__':unittest.main()
