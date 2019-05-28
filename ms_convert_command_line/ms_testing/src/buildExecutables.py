import os, stat, sys, subprocess, copy, argparse, csv

ms_convert_command = [r"C:\Program Files\ProteoWizard\ProteoWizard 3.0.10875\msconvert.exe", "--mzML", "", "--outfile", ""]

pmi_ms_convert_command = [r"..\builds\qt_common-build\bin\Release", "--byspec", "", "--outfile", ""]
cmd_for_byspec_extraction = ["-r", "-t", "1", "-n", "4", "-l", "100", "-d", "0", "-uv", "0.02", "-uf", "c", "-ut", "1000", "-i"]
pmi_pico_console_location = r"..\ms_compress\Release"

def build_executable(file_list, args, branch, prefix):
	print(prefix)
	process = subprocess.Popen(["git", "checkout", args.first_branch if prefix == args.first_prefix else args.second_branch], cwd=r"..\ms_compress")
	process.wait()
	process = subprocess.Popen([r"C:\Users\sam\dev\ms_compress\zBuild.bat"], cwd=r"..\ms_compress")
	process.wait()
	os.rename(r"C:\Users\sam\dev\ms_compress\Release\pmi_pico_console.exe", "C:\\Users\\sam\\dev\\ms_compress\\Release\\" + prefix + "pmi_pico_console.exe")
