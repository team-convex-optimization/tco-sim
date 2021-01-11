import numpy as np
import sys, time, cv2
from PIL import ImageGrab, ImageDraw, ImageFilter, Image
imgCounter = 0


#PIL lib : https://pillow.readthedocs.io/en/stable/reference/ImageChops.html
print("you have 5 seconds to get the game loaded before screen capture!")
time.sleep(3)
while(1):
  im = ImageGrab.grab()

  frame = np.asarray(im) #convert from PIL to CV
  cv2.imwrite("start_"+str(imgCounter)+".png", frame)
    

  frame_gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
  frame_invrt = cv2.bitwise_not(frame_gray)

  kernel = np.ones((100, 100), np.uint8)
  frame_no_holes = cv2.morphologyEx(frame_invrt, cv2.MORPH_CLOSE, kernel)

  ret,frame_track = cv2.threshold(frame_no_holes, 50,255,cv2.THRESH_BINARY_INV)
  frame_lines = cv2.Canny(frame_track, 200, 400)
  cv2.imwrite("end_"+str(imgCounter)+".png", frame_invrt)
  imgCounter += 1




# height, width = frame_lines.shape
# mask = np.zeros_like(frame_lines)
# polygon = np.array([[
#     (0, height / 10),
#     (width, height / 10),
#     (width, height),
#     (0, height),
# ]], np.int32)

# cv2.fillPoly(mask, polygon, 255)
# frame_aoi = cv2.bitwise_and(frame_lines, mask)
# cv2.imshow(' ', frame_aoi)

# # tuning min_threshold, minLineLength, maxLineGap is a trial and error process by hand
# rho = 1  # distance precision in pixel, i.e. 1 pixel
# angle = np.pi / 180  # angular precision in radian, i.e. 1 degree
# min_threshold = 20  # minimal of votes
# line_segments = cv2.HoughLinesP(frame_aoi, rho, angle, min_threshold, 
#                                 np.array([]), minLineLength=200, maxLineGap=10)

# frame_lanes = cv2.cvtColor(frame_aoi.copy(), cv2.COLOR_GRAY2RGB)
# for line in line_segments:
#   for x1,y1,x2,y2 in line:
#     cv2.line(frame_lanes, (x1, y1), (x2, y2), (0, 255, 0), 2)

# cv2.imshow(' ', frame_lanes)

# print(line_segments)
# # line1 = line_segments[0][0]
# # line2 = line_segments[1][0]
# line1 = [7,779,288,310]
# line2 = [1041,243,1405,647]

# # targeted rectangle on original image which needs to be transformed
# tl = [line1[0], line1[1]]
# tr = [line2[2], line2[3]]
# bl = [line1[2], line1[3]]
# br = [line2[0], line2[1]]

# print(tl, tr, br, bl)

# corner_points_array = np.float32([tl,tr,br,bl])

# # Create an array with the parameters (the dimensions) required to build the matrix
# imgTl = [line1[2],line1[3]]
# imgTr = [line2[0],line2[1]]
# imgBl = [line1[2],line1[1]]
# imgBr = [line2[0],line2[3]]
# img_params = np.float32([imgTl,imgTr,imgBr,imgBl])

# # Compute and return the transformation matrix
# matrix = cv2.getPerspectiveTransform(corner_points_array,img_params)
# img_transformed = cv2.warpPerspective(frame_lanes,matrix,(width,height))
# img_transformed = cv2.flip(img_transformed, 0)

# cv2.imshow(' ', img_transformed)
# cv2.imwrite("testend.png", img_transformed)