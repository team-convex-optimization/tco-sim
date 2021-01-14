import time

import mss
import numpy as np
import cv2

with mss.mss() as sct:
    # The screen part to capture
    width = 600
    height = 338
    monitor = {"top": 64, "left": 0, "width": width, "height": height}
    maskCar = np.array( [[[190,338], [220,180], [380,180], [410,338]]], dtype=np.int32 )

    cv2.namedWindow('image')

    while "Screen capturing":
        last_time = time.time()

        # Get raw pixels from the screen, save it to a Numpy array
        image = np.array(sct.grab(monitor), dtype=np.uint8)
        image = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)

        # Sharpen edges
        kernelSharpen = np.array([[-1, -1, -1], [-1, 9, -1], [-1, -1, -1]])
        image = cv2.filter2D(image, -1, kernelSharpen)
        image = cv2.bitwise_not(image)

        # Find edges
        thresh = 300
        image = cv2.Canny(image, thresh, thresh*2, 3)

        # Mask car model with the color in the center of the screen
        image = cv2.fillPoly(image, maskCar, (0, 0, 0))

        # Increase track limit line width
        kernelOnes2x2 = np.ones((2, 2),np.uint8)
        image = cv2.dilate(image, kernelOnes2x2, iterations = 1)

        cv2.imshow('image', image)

        print("fps: {}".format(1 / (time.time() - last_time)))

        # Press "q" to quit
        if cv2.waitKey(25) & 0xFF == ord("q"):
            cv2.destroyAllWindows()
            break