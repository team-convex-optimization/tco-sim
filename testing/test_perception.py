import time
import mss
import numpy as np
import cv2
import math

width = 600
height = 338

# Allow for drawing in one place by queueing draws throughout the processing loop
typeDraw = np.dtype([('pos0', 'i4', 2), ('pos1', 'i4', 2), ('color', 'i4', 3), ('thickness', 'i4')])
queueLine = np.array([], dtype=typeDraw)
queueCirc = np.array([], dtype=typeDraw)
queueRect = np.array([], dtype=typeDraw)

maskCar = np.array([[[190, height], [220, 180], [380, 180], [410, height]]], dtype=np.int32)
kernelOnes2x2 = np.ones((2, 2), np.uint8)
kernelSharpen = np.array([[-1, -1, -1], [-1, 9, -1], [-1, -1, -1]])

def clamp(n, minN, maxN): return max(minN, min(n, maxN))

def queueDrawLine(pos0, pos1, color, thickness):
    global queueLine
    queueLine = np.append(queueLine, np.array([(pos0, pos1, color, thickness)], dtype=typeDraw))

def queueDrawCirc(pos, color, thickness):
    global queueCirc
    queueCirc = np.append(queueCirc, np.array([(pos, 0, color, thickness)], dtype=typeDraw))

def queueDrawRect(pos0, pos1, color, thickness):
    global queueRect
    queueRect = np.append(queueRect, np.array([(pos0, pos1, color, thickness)], dtype=typeDraw))

def clearDrawQueue():
    global queueLine, queueCirc, queueRect
    queueLine = np.array([], dtype=typeDraw)
    queueCirc = np.array([], dtype=typeDraw)
    queueRect = np.array([], dtype=typeDraw)

def drawAllLine(img):
    if queueLine.size != 0:
        for item in np.nditer(queueLine):
            cv2.line(img, tuple(item['pos0'].tolist()), tuple(item['pos1'].tolist()), tuple(item['color'].tolist()), item['thickness'].tolist(), 0, 0)

def drawAllCirc(img):
    if queueCirc.size != 0:
        for item in np.nditer(queueCirc):
            cv2.circle(img, tuple(item['pos0'].tolist()), item['thickness'].tolist(), tuple(item['color'].tolist()), -1)

def drawAllRect(img):
    if queueRect.size != 0:
        for item in np.nditer(queueRect):
            cv2.rectangle(img, tuple(item['pos0'].tolist()), tuple(item['pos1'].tolist()), tuple(item['color'].tolist()), item['thickness'].tolist())

def findLimits(binImg):
    centerWidth = round(width / 2)
    centerHeight = round(height / 2)

    pointLeftDefault = (0, centerHeight)
    pointRightDefault = (width, centerHeight)
    pointLeft = pointLeftDefault
    pointRight = pointRightDefault

    delta = 10
    while delta < centerWidth:
        if pointRight == pointRightDefault and binImg[centerHeight, centerWidth + delta] > 0:
            pointRight = (centerWidth + delta, centerHeight)
        if pointLeft == pointLeftDefault and binImg[centerHeight, centerWidth - delta] > 0:
            pointLeft = (centerWidth - delta, centerHeight)
        if pointRight != pointRightDefault and pointLeft != pointLeftDefault:
            break
        delta += 1

    # TODO: Finding these left and right points can be made more reliable if after the initial
    # search, another vertical search at the extremes is carried out.
    return [pointLeft, pointRight]

# Returns square matrix centered at point (size needs to be odd)
def submatAtPoint(procImg, point, size):
    x, y = point
    sizeH = math.floor(size / 2)
    arr = procImg[y-sizeH:y+1+sizeH,x-sizeH:x+1+sizeH]
    arr = np.reshape(arr, (size, size))
    return arr

# 10.1007/978-3-642-93208-3 (Theo Pavlidis' Algorithm)
# XXX: Fails to trace curves
def limitTracePavlidis(procImg, limit):
    global width, height
    matFront = np.array([[-1,-1],[-1,0],[-1,1]], dtype=np.int16)
    matLeft = np.array([[1,-1],[0,-1],[-1,-1]], dtype=np.int16)
    matRight = np.array([[-1,1],[0,1],[1,1]], dtype=np.int16)
    matRear = np.array([[1,1],[1,0],[1,-1]], dtype=np.int16)
    tracerDir = 'front'
    tracerPos = [limit[0], limit[1]]
    procImg[limit[0]-1, limit[1]] = 0 # Ensure correct start pos condition

    i = 0
    rotInPlace = 0
    matDir = matFront
    dirDelta = 'left'
    while 1:
        queueDrawCirc(tracerPos, (0,255,0), 1)
        offX, offY = tracerPos
        if tracerDir == 'front':
            matDir = matFront
            dirDelta = 'left'
        elif tracerDir == 'left':
            matDir = matLeft
            dirDelta = 'rear'
        elif tracerDir == 'right':
            matDir = matRight
            dirDelta = 'front'
        elif tracerDir == 'rear':
            matDir = matRear
            dirDelta = 'right'
        else:
            raise ValueError
        
        p1 = [clamp(offY + matDir[0][0], 0, height - 1), clamp(offX + matDir[0][1], 0, width - 1)]
        p2 = [clamp(offY + matDir[1][0], 0, height - 1), clamp(offX + matDir[1][1], 0, width - 1)]
        p3 = [clamp(offY + matDir[2][0], 0, height - 1), clamp(offX + matDir[2][1], 0, width - 1)]
        valP1 = procImg[p1[0], p1[1]]
        valP2 = procImg[p2[0], p2[1]]
        valP3 = procImg[p3[0], p3[1]]

        if valP1 > 0:
            rotInPlace = 0
            tracerDir = dirDelta
            tracerPos = [p1[1], p1[0]]
        elif valP2 > 0:
            rotInPlace = 0
            tracerPos = [p2[1], p2[0]]
        elif valP3 > 0:
            rotInPlace = 0
            tracerPos = [p3[1], p3[0]]
        else:
            rotInPlace += 1
            tracerDir = dirDelta
        
        if (rotInPlace > 2) or (i >= 100):
            break
        else:
            i += 1
    return []

def main():
    with mss.mss() as sct:
        # The screen part to capture
        monitor = {"top": 64, "left": 0, "width": width, "height": height}
        cv2.namedWindow('win1')

        while 1:
            last_time = time.time()

            # Get raw pixels from the screen, save it to a Numpy array
            origImg = np.array(sct.grab(monitor), dtype=np.uint8)
            procImg = cv2.cvtColor(origImg, cv2.COLOR_BGR2GRAY)

            # Sharpen edges
            procImg = cv2.filter2D(procImg, -1, kernelSharpen)
            procImg = cv2.bitwise_not(procImg)

            # Find edges
            thresh = 300
            procImg = cv2.Canny(procImg, thresh, thresh*2, 3)

            # Increase track limit line width
            procImg = cv2.dilate(procImg, kernelOnes2x2, iterations=2)

            # Mask car model
            procImg = cv2.fillPoly(procImg, maskCar, (0, 0, 0))

            # Find points on left and right track limits
            [trackLimitLeft, trackLimitRight] = findLimits(procImg)

            # Find all points on left and right track limits
            limitTracePavlidis(procImg, np.array(trackLimitLeft))
            # limitTracePavlidis(procImg, np.array(trackLimitRight))

            # Draw points on original image
            # for point in points:
            #     queueDrawCirc(point, (0, 0, 255), 1)
            queueDrawLine((0, round(height / 2)), (width, round(height / 2)), (0, 0, 255), 1)
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
                break

if __name__ == "__main__":
    main()
