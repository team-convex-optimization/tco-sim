from testPerception import *
from multiprocessing import shared_memory
import posix_ipc as pipc
from struct import Struct
import os

debug = False
chStruct = Struct('Bf')
controlStruct = Struct('BB{}'.format("{}s".format(chStruct.size) * 16))

carState = 0

def carStateNull():
    state = [1, 0]
    i = 0
    while i < 16:
        state.append(tuple([0, 0.0]))
        i += 1
    return state

def readShm(shm, sem):
    data = []
    os.lseek(shm.fd, 0, os.SEEK_SET)
    sem.acquire()
    while True:
        buf = os.read(shm.fd, controlStruct.size)
        if len(buf) != controlStruct.size:
            break
        unpackL1 = controlStruct.unpack_from(buf)
        data.append(unpackL1[0])
        data.append(unpackL1[1])
        i = 0
        while i < 16:
            data.append(chStruct.unpack_from(unpackL1[i + 2]))
            i += 1
    sem.release()
    return data

def writeShm(shm, sem, data):
    i = 0
    while i < 16:
        data[i + 2] = chStruct.pack(data[i + 2][0], data[i + 2][1])
        i += 1
    data = controlStruct.pack(data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7],data[8],data[9],data[10],data[11],data[12],data[13],data[14],data[15],data[16],data[17])
    os.lseek(shm.fd, 0, os.SEEK_SET)
    sem.acquire()
    bytesWritten = 0
    while True:
        bytesWritten += os.write(shm.fd, data[bytesWritten:])
        if bytesWritten >= len(data):
            break
        pass
    sem.release()

def controller():
    global carState

    shm = pipc.SharedMemory("tco_shmem_control")
    sem = pipc.Semaphore("tco_shmem_sem_control")

    if debug:
        cv2.namedWindow('win1')

    carState = carStateNull()
    centerNorm = 0
    centerNormOld = 0
    controlVariable = 0
    pidKP = 0.055 # 0.15
    pidKD = 2.1 # 4
    pidKI = 0.01
    steerFrac = 0.5
    pidProp = 0
    pidDeriv = 0
    pidInteg = 0
    pidVar = 0
    while True:
        last_time = time.time()
        carState[0] = 1

        origImg = grabImage()
        procImg = preProcess(origImg)
        [pointsLeft, pointsRight] = limitsTrace(procImg)
        centerX = round(((pointsRight[0] - pointsLeft[0]) / 2) + pointsLeft[0])
        centerNormOld = centerNorm
        centerNorm = (centerX - round(width/2)) / 200
        if centerNorm > 1.0:
            centerNorm = 1.0
        elif centerNorm < -1.0:
            centerNorm = -1.0
        
        # PID
        pidProp = centerNorm
        pidDeriv = centerNorm - centerNormOld
        pidInteg += pidInteg
        pidVar = pidProp * pidKP
        pidVar += pidDeriv * pidKD
        pidVar += pidInteg * pidKI


        steerFrac += pidVar/2.0
        if steerFrac < 0.0:
            steerFrac = 0.0
        elif steerFrac > 1.0:
            steerFrac = 1.0

        carState[3] = (int(1), float(steerFrac))

        if abs(steerFrac - 0.5) > 0.2:
            carState[2] = (int(1), float(0.52))
        else:
            carState[2] = (int(1), float(0.56))

        writeShm(shm,sem, carState)

        # Draw points on original image
        if debug:
            queueDrawLine((round(width/2), round(height/2)), (centerX, 0), (0,0,255), 1)
            procImg = cv2.cvtColor(procImg, cv2.COLOR_GRAY2BGR)
            drawAllLine(procImg)
            drawAllCirc(procImg)
            drawAllRect(procImg)
            clearDrawQueue()
            cv2.imshow('win1', procImg)

        print("fps: {}".format(1 / (time.time() - last_time)))

        # Press "q" to quit
        if cv2.waitKey(25) & 0xFF == ord("q"):
            cv2.destroyAllWindows()
            writeShm(shm,sem, carStateNull())
            break

def main():
    controller()

if __name__ == "__main__":
    main()
