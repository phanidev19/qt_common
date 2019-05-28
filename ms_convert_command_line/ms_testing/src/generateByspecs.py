import os, stat, sys, subprocess, copy, argparse, csv

ms_convert_command = [r"C:\Program Files\ProteoWizard\ProteoWizard 3.0.10875\msconvert.exe", "--mzML", "", "--outfile", ""]
pmi_ms_convert_command = [r"C:\Users\sam\dev\builds\qt_common-build\bin\Release\pwiz-pmi\msconvert-pmi.exe", "--byspec", "", "--outfile", ""]
cmd_for_byspec_extraction = ["-r", "-t", "1", "-n", "4", "-l", "100", "-d", "0", "-uv", "0.02", "-uf", "c", "-ut", "1000", "-i"]

def get_byspec2_from_raw(file, full_file_path, args, prefix, n_value, is_pwiz):
	true_byspec_path = args.output_path_for_byspecs + "\\" + (n_value if not is_pwiz else "pwiz_pico")
	if not os.path.exists(true_byspec_path):
		os.makedirs(true_byspec_path)

	if is_pwiz and prefix == args.second_prefix:
		n_value = '2'
	if not is_pwiz or prefix == args.second_prefix:
		tmp_cmd = []
		tmp_cmd.append("C:\\Users\\sam\\dev\\ms_compress\\Release\\" + prefix + "pmi_pico_console.exe")
		tmp_cmd.extend(cmd_for_byspec_extraction)
		tmp_cmd[5] = n_value
		tmp_cmd.extend([full_file_path, "-o",
			true_byspec_path + "\\" + prefix + n_value + "_" + os.path.basename(file) + ".byspec2"
			if not is_pwiz 
			else true_byspec_path + "\\" + os.path.basename(file) + ".pico.byspec2"])
		print(tmp_cmd)
		process = subprocess.Popen(tmp_cmd)
		process.wait()
	else:
		tmp_cmd = copy.deepcopy(ms_convert_command)
		tmp_cmd[2] = full_file_path
		tmp_cmd[4] = full_file_path + ".pwiz.mzML"
		process = subprocess.Popen(tmp_cmd, cwd=true_byspec_path)
		process.wait()
		tmp_cmd = copy.deepcopy(pmi_ms_convert_command)
		tmp_cmd[2] = true_byspec_path + "\\" + file + ".pwiz.mzML"
		tmp_cmd[4] = true_byspec_path + "\\" + file + ".pwiz.byspec2"
		process = subprocess.Popen(tmp_cmd, cwd=true_byspec_path)
		process.wait()
		os.remove(true_byspec_path + "\\" + file + ".pwiz.mzML")
	
def generate_byspecs(file_list, args, prefix, n_value, is_pwiz):
	for path_name in file_list:
		for file in os.listdir(path_name):
			get_byspec2_from_raw(file, path_name + "\\" + file, args, prefix, n_value, is_pwiz)