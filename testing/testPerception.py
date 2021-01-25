import time
import mss
import numpy as np
import cv2
import math

width = 600
height = 338
debug = False

# Where the simulator window is (upper left corner)
simWin = {"top": 64, "left": 0, "width": width, "height": height}

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

def findLimits(procImg):
    global width, height
    centerWidth = round(width / 2)
    centerHeight = round(height / 2)

    heinghtStart = centerHeight

    pointLeftDefault = (0, heinghtStart)
    pointRightDefault = (width, heinghtStart)
    pointLeft = pointLeftDefault
    pointRight = pointRightDefault

    delta = 0
    offset = 0
    while delta < centerWidth:
        rightX = clamp(centerWidth - offset + delta, 0, width - 1)
        leftx = clamp(centerWidth + offset - delta, 0, width - 1)
        if pointRight == pointRightDefault and procImg[heinghtStart, rightX] > 0:
            pointRight = (rightX, heinghtStart)
        if pointLeft == pointLeftDefault and procImg[heinghtStart, leftx] > 0:
            pointLeft = (leftx, heinghtStart)
        if pointRight != pointRightDefault and pointLeft != pointLeftDefault:
            break
        delta += 1

    # TODO: Finding these left and right points can be made more reliable if after the initial
    # search, another vertical search at the extremes is carried out.
    return [pointLeft, pointRight]

def circleDeltaPoints(centerX, centerY, radius):
    pointsQ4U = []
    pointsQ4D = []
    r = radius

    front = [[0, -r]]
    left = [[-r, 0]]
    right = [[r, 0]]
    rear = [[0, r]]

    x = r
    y = 0
    p = 1 - r
    while x > y:
        y += 1
    
        if p <= 0:  
            p = p + (2 * y) + 1
        else:          
            x -= 1
            p = p + (2 * y) - (2 * x) + 1
        
        if (x < y):
            break

        pointsQ4U.append([x, y])
        if x != y:
            pointsQ4D.append([y, x])
    pointsQ4D.reverse()
    pointsQ4 = pointsQ4U + pointsQ4D
    pointsQ1 = [[-pt[0], -pt[1]] for pt in pointsQ4]
    pointsQ1.reverse()
    pointsQ2 = [[pt[0], -pt[1]] for pt in pointsQ4]
    pointsQ3 = [[-pt[0], pt[1]] for pt in pointsQ4]
    pointsQ4.reverse()
    points = front + pointsQ1 + left + pointsQ3 + rear + pointsQ4 + right + pointsQ2
    return points

def wrapAround(val, valMin, valMax):
    if val == valMin:
        val = valMax
    else:
        val %= valMax
    return val

# Version of Radial Sweep which is faster but produces a rough trace
def radialSweepFast(procImg, limit, clockwise, armLength):
    dirDelta = circleDeltaPoints(limit[0], limit[1], int(armLength/2))
    quadrantSize = int((len(dirDelta) - 4) / 4) # This is always a whole number
    idxLeft = quadrantSize + 1
    idxRight = (3 * quadrantSize) + 3

    tracerPos = [limit[0], limit[1]]
    points = np.array([], dtype=np.uint16)

    if not clockwise:
        tracerDirIdx = idxRight # Right
    else:
        tracerDirIdx = idxLeft # Left

    pointsFound = 0
    maxPoints = 50
    x = 0
    y = 0
    inPlaceRot = 0
    while True:
        points = np.append(points, tracerPos)
        if debug:
            queueDrawCirc(tracerPos, (0,0,255), 1)
        y = tracerPos[1] + dirDelta[tracerDirIdx][1]
        x = tracerPos[0] + dirDelta[tracerDirIdx][0]
        if (y + 10 >= height) or (y - 10 < 0) or (x + 10 >= width) or (x - 10 < 0):
            break
        if procImg[y,x] > 0:
            tracerPos = [x, y]
            inPlaceRot = 0
            tracerDirIdx = int(wrapAround(tracerDirIdx + quadrantSize * 2 + 2, 0, len(dirDelta) - 1)) # Same for both CW and CCW since its a 180 deg rotation
            # Rotate an extra 90 degrees to avoid checking the pixel previously marked with a point and to speed things up
            # This is a heuristic so might break if there is a sharp turn into the direction opposite to "clockwise" argument
            if not clockwise:
                tracerDirIdx = int(wrapAround(tracerDirIdx + 1 + quadrantSize, 0, len(dirDelta) - 1))
            else:
                tracerDirIdx = int(wrapAround(tracerDirIdx - 1 - quadrantSize, 0, len(dirDelta) - 1))
            pointsFound += 1
        else:
            # This shows all the points that were radially sweeped ;)
            if debug:
                queueDrawCirc([x,y], (155,155,0), 0)
            inPlaceRot += 1
        
        if not clockwise:
            tracerDirIdx = int(wrapAround(tracerDirIdx + 1, 0, len(dirDelta) - 1))
        else:
            tracerDirIdx = int(wrapAround(tracerDirIdx - 1, 0, len(dirDelta) - 1))

        if inPlaceRot >= len(dirDelta)-1 or (pointsFound >= maxPoints):
            break
        
    return np.reshape(points, (int(len(points) / 2),2))

def preProcess(img):
    procImg = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

    # Sharpen edges
    procImg = cv2.filter2D(procImg, -1, kernelSharpen)
    procImg = cv2.bitwise_not(procImg)

    # Find edges
    thresh = 300
    procImg = cv2.Canny(procImg, thresh, thresh*2, 3)

    # Increase track limit line widthq
    procImg = cv2.dilate(procImg, kernelOnes2x2, iterations=3)

    # Mask car model
    procImg = cv2.fillPoly(procImg, maskCar, (0, 0, 0))
    return procImg

def limitsTrace(procImg):
    # Find points on left and right track limits
    [trackLimitLeft, trackLimitRight] = findLimits(procImg)

    # Find all points on left and right track limits
    pointsLeft = radialSweepFast(procImg, np.array(trackLimitLeft), 0, 20)
    pointsRight = radialSweepFast(procImg, np.array(trackLimitRight), 1, 20)

    return [pointsLeft, pointsRight]

def grabImage():
    # Get raw pixels from the screen, save it to a Numpy array
    return np.array(mss.mss().grab(simWin), dtype=np.uint8)

def main():
    if debug:
        cv2.namedWindow('win1')

    while 1:
        last_time = time.time()

        origImg = grabImage()
        procImg = preProcess(origImg)
        [pointsLeft, pointsRight] = limitsTrace(procImg)

        # Draw points on original image
        if debug:
            for point in pointsLeft:
                queueDrawCirc(point, (0, 0, 255), 1)
            for point in pointsRight:
                queueDrawCirc(point, (0, 0, 255), 1)
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
