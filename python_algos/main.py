import sys
import ctypes
import ctypes.wintypes

from dataclasses import dataclass


@dataclass
class Vec2:
    x: float = 0.0
    y: float = 0.0

    def __getitem__(self, index: int) -> float:
        return (self.x, self.y)[index]

    def to_ctypes(self) -> ctypes.Array[ctypes.c_float]:
        return (ctypes.c_float * 2)(self.x, self.y)

@dataclass
class Vec3:
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0

    def __getitem__(self, index: int) -> float:
        return (self.x, self.y, self.z)[index]

    def to_ctypes(self) -> ctypes.Array[ctypes.c_float]:
        return (ctypes.c_float * 3)(self.x, self.y, self.z)

class SharedData(ctypes.Structure):
    _fields_ = [
        ("requestType", ctypes.c_int),
        ("pos",         ctypes.c_float * 3),
        ("moveDir",     ctypes.c_float * 2),
        ("result",      ctypes.c_float),
    ]
    requestType: int
    pos:         ctypes.Array[ctypes.c_float]
    moveDir:     ctypes.Array[ctypes.c_float]
    result:      float



shm_name = b"g_code_optimizer2_shm"

if sys.platform == "win32":
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

    # Constants
    FILE_MAP_READ        = 0x0004
    FILE_MAP_WRITE       = 0x0002
    FILE_MAP_ALL_ACCESS  = FILE_MAP_READ | FILE_MAP_WRITE
    SEMAPHORE_ALL_ACCESS = 0x1F0003
    INFINITE             = 0xFFFFFFFF

    # Function signatures
    kernel32.OpenFileMappingA.restype  = ctypes.wintypes.HANDLE
    kernel32.OpenFileMappingA.argtypes = [ctypes.wintypes.DWORD, ctypes.wintypes.BOOL, ctypes.c_char_p]

    kernel32.MapViewOfFile.restype  = ctypes.c_void_p
    kernel32.MapViewOfFile.argtypes = [ctypes.wintypes.HANDLE, ctypes.wintypes.DWORD,
                                       ctypes.wintypes.DWORD, ctypes.wintypes.DWORD, ctypes.c_size_t]

    kernel32.UnmapViewOfFile.restype  = ctypes.wintypes.BOOL
    kernel32.UnmapViewOfFile.argtypes = [ctypes.c_void_p]

    kernel32.OpenSemaphoreA.restype  = ctypes.wintypes.HANDLE
    kernel32.OpenSemaphoreA.argtypes = [ctypes.wintypes.DWORD, ctypes.wintypes.BOOL, ctypes.c_char_p]

    kernel32.ReleaseSemaphore.restype  = ctypes.wintypes.BOOL
    kernel32.ReleaseSemaphore.argtypes = [ctypes.wintypes.HANDLE, ctypes.wintypes.LONG, ctypes.wintypes.LPLONG]

    kernel32.WaitForSingleObject.restype  = ctypes.wintypes.DWORD
    kernel32.WaitForSingleObject.argtypes = [ctypes.wintypes.HANDLE, ctypes.wintypes.DWORD]

    kernel32.CloseHandle.restype  = ctypes.wintypes.BOOL
    kernel32.CloseHandle.argtypes = [ctypes.wintypes.HANDLE]

    class Algo:
        def __init__(self):
            self._hMapFile = kernel32.OpenFileMappingA(FILE_MAP_ALL_ACCESS, False, shm_name)
            if not self._hMapFile:
                raise RuntimeError("Could not open shared memory. Is the C++ worker running?")

            self._data_ptr = kernel32.MapViewOfFile(self._hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, ctypes.sizeof(SharedData))
            if not self._data_ptr:
                raise RuntimeError("Could not map view of file.")
            self._data = SharedData.from_address(self._data_ptr)  # built on top of the raw pointer

            self._sem_req = kernel32.OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, False, b"sem_request")
            self._sem_res = kernel32.OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, False, b"sem_response")
            if not self._sem_req or not self._sem_res:
                raise RuntimeError("Could not open semaphores. Is the C++ worker running?")

            kernel32.WaitForSingleObject(self._sem_res, 0)  # Ensure the response semaphore is initially empty
            self.request_for_pos(Vec3(0.0, 0.0, 1.0))  # Move to default position

        def _synchronize(self) -> float:
            kernel32.ReleaseSemaphore(self._sem_req, 1, None)
            kernel32.WaitForSingleObject(self._sem_res, INFINITE)
            return self._data.result

        def request_for_pos(self, pos: Vec3) -> float:
            self._data.requestType = 1
            self._data.pos = pos.to_ctypes()
            return self._synchronize()

        def request_for_move(self, move_dir: Vec2) -> float:
            self._data.requestType = 2
            self._data.moveDir = move_dir.to_ctypes()
            return self._synchronize()

    def __del__(self):
        kernel32.CloseHandle(self._sem_req)
        kernel32.CloseHandle(self._sem_res)
        kernel32.UnmapViewOfFile(self._data_ptr)
        kernel32.CloseHandle(self._hMapFile)

else:  # Linux / macOS
    raise NotImplementedError("This code is only implemented for Windows.")

if __name__ == "__main__":
    algo = Algo()
    while True:
        algo.request_for_move(Vec2(1.0, 0.0))

