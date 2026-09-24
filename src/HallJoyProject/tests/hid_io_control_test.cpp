#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#include <hidclass.h>
#include <cassert>
#include <cstdint>
#include <iostream>
static unsigned calls=0,cancels=0,reaps=0;
static bool pending=false,cancelled=false;
static OVERLAPPED* owned=nullptr;
static BOOL WINAPI Control(HANDLE,DWORD code,void* input,DWORD inlen,
                           void* output,DWORD outlen,DWORD*,OVERLAPPED* ov){
  ++calls;owned=ov;
  assert(input && inlen==65);
  if(code==IOCTL_HID_GET_FEATURE){assert(output==input && outlen==65);}
  else {assert(code==IOCTL_HID_SET_FEATURE && !output && !outlen);}
  if(pending){SetLastError(ERROR_IO_PENDING);return FALSE;}
  return TRUE;
}
static BOOL WINAPI Reap(HANDLE,OVERLAPPED* ov,DWORD* bytes,BOOL wait){
  assert(owned==ov);++reaps;
  if(cancelled){SetLastError(ERROR_OPERATION_ABORTED);return FALSE;}
  if(pending && !wait){SetLastError(ERROR_IO_INCOMPLETE);return FALSE;}
  *bytes=64;return TRUE;
}
static BOOL WINAPI Cancel(HANDLE,OVERLAPPED* ov){assert(ov==owned);++cancels;cancelled=true;return TRUE;}
#define DeviceIoControl Control
#define GetOverlappedResult Reap
#define CancelIoEx Cancel
#include "hid_io_operation.h"
#undef DeviceIoControl
#undef GetOverlappedResult
#undef CancelIoEx
int main(){
  unsigned char buffer[65]{};DWORD error=0,bytes=0;
  const auto h=reinterpret_cast<HANDLE>(std::uintptr_t{123});
  {
    HidIoOperation op(h);assert(op.IsValid());
    assert(op.StartControl(IOCTL_HID_GET_FEATURE,buffer,65,buffer,65,&error)==HidIoOperation::StartResult::Completed);
    assert(op.Finish(&bytes,&error,false) && bytes==64);
    assert(!op.Finish(&bytes,&error,false));
  }
  pending=true;
  {
    HidIoOperation op(h);
    assert(op.StartControl(IOCTL_HID_SET_FEATURE,buffer,65,nullptr,0,&error)==HidIoOperation::StartResult::Pending);
    assert(op.StartControl(IOCTL_HID_SET_FEATURE,buffer,65,nullptr,0,&error)==HidIoOperation::StartResult::Failed);
    assert(op.Wait(0)==WAIT_TIMEOUT);
    assert(!op.Finish(&bytes,&error,false) && error==ERROR_IO_INCOMPLETE);
    assert(op.CancelAndDrain(&bytes,&error) && error==ERROR_OPERATION_ABORTED);
  }
  assert(calls==2 && cancels==1 && reaps==3);
  cancelled=false;
  {
    HidIoOperation op(h);
    assert(op.StartControl(IOCTL_HID_GET_FEATURE,buffer,65,buffer,65,&error)==HidIoOperation::StartResult::Pending);
  }
  assert(cancels==2 && reaps==4);
  std::cout<<"HID_IO_CONTROL=PASS completion pending cancel drain destructor\n";
}
