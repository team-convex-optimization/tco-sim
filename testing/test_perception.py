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

def clamp(n, minN, maxN): return max(minN, min(n, maxN))

def queueDrawLine(pos0, pos1, color, thickness):
    global queueLine
    queueLine = np.append(queueLine, np.array([(pos0, pos1, color, thickness)], dtype=typeDraw))

def queueDrawCirc(pos0, pos1, color, thickness):
    global queueCirc
    queueCirc = np.append(queueCirc, np.array([(pos0, pos1, color, thickness)], dtype=typeDraw))

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
    centerWidth = int(width / 2)
    centerHeight = int(height / 2)

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
    return (pointLeft, pointRight)

def slidingWindowPoints(origImg, procImg, trackLimitLeft, trackLimitRight):
    lastVec = (0, 0)
    limitLeftLast = trackLimitLeft
    limitRigthLast = trackLimitRight

    delta = 0
    # 'sWin' for sliding window
    sWinWidth = 10
    sWinWidthH = int(sWinWidth/2)
    sWinHeight = 10
    sWinHeightH = int(sWinHeight/2)
    while delta < 10:
        rectTopLeft = (limitLeftLast[0] - sWinWidthH, limitLeftLast[1] - sWinHeightH)
        rectBotRight = (limitLeftLast[0] + sWinWidthH, limitLeftLast[1] + sWinHeightH)
        queueDrawRect(rectTopLeft, rectBotRight, np.array([0, 255, 0], dtype=np.int32), 1)
        rect = procImg[rectTopLeft[1]:rectBotRight[1], rectTopLeft[0]:rectBotRight[0]]
        print(rect)
        points = cv2.findNonZero(rect)

        if points is not None:
            # Find line that fits the white points on rect matrix
            vx, vy, x, y = cv2.fitLine(points, cv2.DIST_L2, 0, 0.01, 0.01)
            x0 = rectBotRight[0] - sWinWidthH
            y0 = rectBotRight[1] - sWinHeightH
            x1 = x0 + round(vx[0] * sWinWidth)
            y1 = y0 + round(vy[0] * sWinHeight)
            
            queueDrawLine((x0, y0), (x1, y1), (0, 0, 255), 2)
            limitLeftLast = (x1, y1)
            lastVec = (vx, vy)
        delta += 1
    return []

def main():
    with mss.mss() as sct:
        # The screen part to capture
        monitor = {"top": 64, "left": 0, "width": width, "height": height}
        maskCar = np.array([[[190, height], [220, 180], [380, 180], [410, height]]], dtype=np.int32)
        kernelOnes2x2 = np.ones((2, 2), np.uint8)
        kernelSharpen = np.array([[-1, -1, -1], [-1, 9, -1], [-1, -1, -1]])

        cv2.namedWindow('win1')

        while "Screen capturing":
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
            procImg = cv2.dilate(procImg, kernelOnes2x2, iterations=1)

            # Mask car model
            procImg = cv2.fillPoly(procImg, maskCar, (0, 0, 0))

            # Find points on left and right track limits
            trackLimitLeft, trackLimitRight = findLimits(procImg)

            # Find all points on left and right track limits
            points = slidingWindowPoints(origImg, procImg, trackLimitLeft, trackLimitRight)

            # Draw points on original image
            for point in points:
                queueDrawCirc(point, 0, (0, 0, 255), 1)
            queueDrawCirc(trackLimitLeft, 0, (0, 0, 255), 1)
            queueDrawCirc(trackLimitRight, 0, (0, 0, 255), 1)

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
