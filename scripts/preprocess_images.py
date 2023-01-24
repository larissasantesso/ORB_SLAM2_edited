import sys
import cv2
import re
import numpy as np
import glob

def preprocess(input_folder, resize_width, resize_height, output_folder):
  
  for image_item in glob.glob(input_folder+"/*"):
    image_name = str(re.split('/',str(image_item))[-1]))
    # Read the image 
    image = cv2.imread(image_item,, cv2.IMREAD_UNCHANGED)
    
    # resize image to new  width and height
    image_resized = cv2.resize(image, (input_width, input_height), interpolation=cv2.INTER_LINEAR)
    cv2.imwrite(output_folder+"/"+image_name,image_resized) 
  
if __name__ == "__main__":
    if sys.argv[1]:
      preprocess(sys.argv[1],sys.argv[2], sys.argv[3], sys.argv[4])
    else:
      print("Error: Input variables should be in the sequence:")
      raise ValueError("Input variables must follow the sequence: images_folder resize_to_width resize_to_height path_new_folder")
  
  
