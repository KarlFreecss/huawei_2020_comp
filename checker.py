# coding: utf-8
#================================================================#
#   Copyright (C) 2020 Freecss All rights reserved.
#   
#   File Name     ：checker.py
#   Author        ：freecss
#   Email         ：karlfreecss@gmail.com
#   Created Date  ：2020/04/10
#   Description   ：
#
#================================================================#

import os

data_path = "test_data_set"
#exe = "opt_0409"
#exe = "demo"
exe = "opt_0410"
exe = "opt_0411"

print("Test " + exe)
for root, dirs, files in os.walk(data_path):
    for dir_name in dirs:
        test_file_source_dir = os.path.join(root, dir_name)
        test_file_source_path = os.path.join(test_file_source_dir, "test_data.txt")
        test_result_source_path = os.path.join(test_file_source_dir, "result.txt")
        test_file_target_path = "/data/test_data.txt"
        test_result_target_path = "std_result.txt"
        exe_output = "/projects/student/result.txt"
        print("Test", test_file_source_path)

        os.system("cp %s %s" % (test_file_source_path, test_file_target_path))
        os.system("cp %s %s" % (test_result_source_path, test_result_target_path))

        os.system("g++ -O3 %s.cpp -o %s -lpthread" % (exe, exe))
        os.system("time ./%s" % exe)
        os.system("diff %s %s > %s.result.txt" % (test_result_target_path, exe_output, dir_name))



