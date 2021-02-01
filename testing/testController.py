from testPerception import *
import posix_ipc as pipc
from struct import Struct
import os

debug = True
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
    data = controlStruct.pack(*data)
    os.lseek(shm.fd, 0, os.SEEK_SET)
    sem.acquire()
    bytesWritten = 0
    while True:
        bytesWritten += os.write(shm.fd, data[bytesWritten:])
        if bytesWritten >= len(data):
            break
        pass
    sem.release()

def distToTurn(procImg, centerX, centerY):
    dist = 0
    while True:
        if procImg[abs(centerY - dist), centerX] > 0:
            return dist
        dist += 1
        if centerY - dist < 0:
            return dist

def controller():
    global carState

    shm = pipc.SharedMemory("tco_shmem_control")
    sem = pipc.Semaphore("tco_shmem_sem_control")

    cv2.namedWindow('win1')

    carState = carStateNull()
    crossTrackErr = 0
    crossTrackErrOld = 0
    crossTrackErrRate = 0
    controlVariable = 0
    pidKP = 0.01
    pidKD = 0.02
    steerFrac = 0
    while True:
        last_time = time.time()
        carState[0] = 1

        origImg = grabImage()
        procImg = preProcess(origImg)
        [pointsLeft, pointsRight] = limitsTrace(procImg)

        # Find cross track error
        centerX = round(((pointsRight[0][0] - pointsLeft[0][0]) / 2) + pointsLeft[0][0])
        crossTrackErrOld = crossTrackErr
        crossTrackErr = centerX - round(width/2)
        crossTrackErrRate = crossTrackErr - crossTrackErrOld
        
        # PID
        steerFrac = crossTrackErr * pidKP
        steerFrac += crossTrackErrRate * pidKD
        steerFrac = (steerFrac / 2.0) + 0.5 # To go from range -1 to 1 to range 0 to 1
        if steerFrac < 0.0:
            steerFrac = 0.0
        elif steerFrac > 1.0:
            steerFrac = 1.0

        # Update steering channel
        carState[3] = (int(1), float(steerFrac))

        # Update throttle channel
        if distToTurn(procImg, centerX, round(height / 2)) < 150:
            carState[2] = (int(1), float(0.58))
        else:
            carState[2] = (int(1), float(1.0))

        writeShm(shm,sem, carState)

        # Draw points on original image
        if debug:
            for pt in pointsLeft:
                queueDrawCirc(pt, (0,0,255), 3)
            for pt in pointsRight:
                queueDrawCirc(pt, (0,0,255), 3)
            queueDrawCirc((round((pointsRight[0][0] - pointsLeft[0][0])/2 + pointsLeft[0][0]), 319), (0,255,0), 3)
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
