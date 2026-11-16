[CmdletBinding()]
param([Parameter(Mandatory=$true)][int]$AttachProcessId)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# One named job per Windows session: nested build entry points join the SAME
# budget, so limits do not multiply (90% of90%). No kill-on-close or affinity.
if (-not ('HallJoy.BuildCpuBudget' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
namespace HallJoy {
    public static class BuildCpuBudget {
        const string Name = @"Local\HallJoy.BuildCpuBudget90.v1";
        [StructLayout(LayoutKind.Sequential)]
        struct CpuInfo { public uint Flags, Rate; }
        [StructLayout(LayoutKind.Sequential)]
        struct BasicLimits {
            public long ProcessTime, JobTime;
            public uint Flags;
            public UIntPtr MinWorkingSet, MaxWorkingSet;
            public uint ActiveProcesses;
            public UIntPtr Affinity;
            public uint Priority, SchedulingClass;
        }
        [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
        static extern IntPtr CreateJobObjectW(IntPtr attributes, string name);
        [DllImport("kernel32.dll", SetLastError=true)]
        static extern bool SetInformationJobObject(IntPtr job, int type, ref CpuInfo info, uint size);
        [DllImport("kernel32.dll", SetLastError=true)]
        static extern bool SetInformationJobObject(IntPtr job, int type, ref BasicLimits info, uint size);
        [DllImport("kernel32.dll", SetLastError=true)]
        static extern bool QueryInformationJobObject(IntPtr job, int type, out CpuInfo info, uint size, IntPtr returned);
        [DllImport("kernel32.dll", SetLastError=true)]
        static extern bool QueryInformationJobObject(IntPtr job, int type, out BasicLimits info, uint size, IntPtr returned);
        [DllImport("kernel32.dll", SetLastError=true)]
        static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
        [DllImport("kernel32.dll", SetLastError=true)]
        static extern bool IsProcessInJob(IntPtr process, IntPtr job, out bool result);
        [DllImport("kernel32.dll", SetLastError=true)]
        static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);
        [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
        [DllImport("kernel32.dll", SetLastError=true)]
        static extern bool DuplicateHandle(IntPtr sourceProcess, IntPtr source, IntPtr targetProcess, out IntPtr target, uint access, bool inherit, uint options);
        [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
        static void Check(bool ok) { if (!ok) throw new Win32Exception(Marshal.GetLastWin32Error()); }
        public static ulong Attach(int pid) {
            IntPtr job=CreateJobObjectW(IntPtr.Zero,Name);
            Check(job!=IntPtr.Zero);
            try {
                var cpu=new CpuInfo { Flags=5, Rate=9000 }; // ENABLE|HARD_CAP
                Check(SetInformationJobObject(job,15,ref cpu,(uint)Marshal.SizeOf(typeof(CpuInfo))));
                var limits=new BasicLimits { Flags=0x20, Priority=0x4000 }; // BELOW_NORMAL
                Check(SetInformationJobObject(job,2,ref limits,(uint)Marshal.SizeOf(typeof(BasicLimits))));
                IntPtr process=OpenProcess(0x1341,false,pid); // query, set/quota, terminate
                Check(process!=IntPtr.Zero);
                try {
                    bool member;
                    Check(IsProcessInJob(process,job,out member));
                    if (!member) Check(AssignProcessToJobObject(job,process));
                    Check(IsProcessInJob(process,job,out member));
                    if (!member) throw new InvalidOperationException("Build did not join CPU budget");
                    CpuInfo actual; BasicLimits actualLimits;
                    Check(QueryInformationJobObject(job,15,out actual,(uint)Marshal.SizeOf(typeof(CpuInfo)),IntPtr.Zero));
                    Check(QueryInformationJobObject(job,2,out actualLimits,(uint)Marshal.SizeOf(typeof(BasicLimits)),IntPtr.Zero));
                    if(actual.Flags!=5 || actual.Rate!=9000 || actualLimits.Flags!=0x20 || actualLimits.Priority!=0x4000)
                        throw new InvalidOperationException("Build CPU budget readback differs");
                    IntPtr retained;
                    Check(DuplicateHandle(GetCurrentProcess(),job,process,out retained,0,false,2));
                    return unchecked((ulong)retained.ToInt64());
                } finally { CloseHandle(process); }
            } finally { CloseHandle(job); }
            // The target retains a non-inherited handle until Python exits.
            // The calling shell and installed app publisher are not assigned.
        }
    }
}
'@
}
$retainedHandle = [HallJoy.BuildCpuBudget]::Attach($AttachProcessId)
Write-Output "BUILD_CPU_BUDGET=90 priority=BelowNormal pid=$AttachProcessId readback=1 handle=$retainedHandle"
