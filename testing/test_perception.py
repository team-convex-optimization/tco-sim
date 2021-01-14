import time

import mss
import numpy as np
import cv2
import math

def clamp(n, minN, maxN): return max(minN, min(n, maxN))

with mss.mss() as sct:
    # The screen part to capture
    width = 600
    height = 338
    monitor = {"top": 64, "left": 0, "width": width, "height": height}
    maskCar = np.array([[[190, 338], [220, 180], [380, 180], [410, 338]]], dtype=np.int32)
    kernelOnes2x2 = np.ones((2, 2), np.uint8)
    kernelSharpen = np.array([[-1, -1, -1], [-1, 9, -1], [-1, -1, -1]])

    cv2.namedWindow('win1')
    cv2.namedWindow('win2')


    while "Screen capturing":
        last_time = time.time()

        # Get raw pixels from the screen, save it to a Numpy array
        original = np.array(sct.grab(monitor), dtype=np.uint8)
        image = cv2.cvtColor(original, cv2.COLOR_BGR2GRAY)

        # Sharpen edges
        image = cv2.filter2D(image, -1, kernelSharpen)
        image = cv2.bitwise_not(image)

        # Find edges
        thresh = 300
        image = cv2.Canny(image, thresh, thresh*2, 3)

        # Increase track limit line width
        image = cv2.dilate(image, kernelOnes2x2, iterations=1)

        # Mask car model
        image = cv2.fillPoly(image, maskCar, (0, 0, 0))

        cv2.imshow('win1', image)

        # Find left and right track limit
        centerWidth = int(width / 2)
        centerHeight = int(height / 2)

        pointLeftDefault = (0, centerHeight)
        pointRightDefault = (centerWidth, centerHeight)
        pointLeft = pointLeftDefault
        pointRight = pointRightDefault
        delta = 10
        while delta < centerWidth:
            if pointRight == pointRightDefault and image[centerHeight, centerWidth + delta] > 0:
                pointRight = (centerWidth + delta, centerHeight)
            if pointLeft == pointLeftDefault and image[centerHeight, centerWidth - delta] > 0:
                pointLeft = (centerWidth - delta, centerHeight)
            if pointRight != pointRightDefault and pointLeft != pointLeftDefault:
                break
            delta += 1
        # TODO: Finding these left and right points can be made more reliable if after the initial
        # search, another vertical search at the extremes is carried out.

        # Find all points on left and right track limits
        pointLeftLast = pointLeft
        pointRightLast = pointRight
        delta = 0
        sWinWidth = 10
        sWinWidthH = int(sWinWidth/2)
        sWinHeight = 10
        sWinHeightH = int(sWinHeight/2)
        while 1:
            delta += 1
            rectTopLeft = (pointLeftLast[0] - sWinWidthH, pointLeftLast[1] - sWinHeightH)
            rectBotRight = (pointLeftLast[0] + sWinWidthH, pointLeftLast[1] + sWinHeightH)
            cv2.rectangle(original, rectTopLeft, rectBotRight, (0, 255, 0), 1)
            rect = image[rectTopLeft[1]:rectBotRight[1], rectTopLeft[0]:rectBotRight[0]]
            # TODO: Find orientation of the line in rect and then find pos of next window using it
            break

        cv2.circle(original, pointLeft, 1, (0, 255, 0), -1)
        cv2.circle(original, pointRight, 1, (0, 255, 0), -1)

        cv2.imshow('win2', original)

        print("fps: {}".format(1 / (time.time() - last_time)))

        # Press "q" to quit
        if cv2.waitKey(25) & 0xFF == ord("q"):
            cv2.destroyAllWindows()
            break
