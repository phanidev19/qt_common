import csv, os, copy, subprocess

PMi_MSConvert_exe_location = r"C:\Users\sam\dev\builds\qt_common-build\bin\Release"
cmd_for_comparison = [PMi_MSConvert_exe_location + r"\PMi-MSConvert.exe", "--command", "6"]

def compare_one_pair(file, file_partner, args, n_value):
	tmp_cmd = copy.deepcopy(cmd_for_comparison)
	tmp_cmd.extend([os.path.abspath(file), os.path.abspath(file_partner), "-o", os.path.abspath(args.output_path_for_comparisons + "\\" + (n_value if n_value != '7' else "pwiz_pico"))])
	print(tmp_cmd)
	process = subprocess.Popen(tmp_cmd, cwd=PMi_MSConvert_exe_location)
	process.wait()

def find_matching_file(file, file_list, args, n_value):
	for file_partner in file_list:
		if n_value == '7':
			if "pico" in file_partner:
				if file[:file.index("pwiz")] == file_partner[:file_partner.index("pico")]:
					return file_partner
		else:
			if file[len(args.first_prefix):] == file_partner[len(args.second_prefix):] and file != file_partner:
				return file_partner

def compare_files(args, n_value):
	true_output_path = args.output_path_for_comparisons + "\\" + (n_value if n_value != '7' else "pwiz_pico")
	true_byspec_path = args.output_path_for_byspecs + "\\" + (n_value if n_value != '7' else "pwiz_pico")
	if not os.path.exists(true_output_path):
		os.makedirs(true_output_path)
	file_list = os.listdir(true_byspec_path)
	for file in file_list:
		if n_value == '7':
			if "pwiz" in file:
				file_partner = find_matching_file(file, file_list, args, n_value)
				compare_one_pair(true_byspec_path + "\\" + file,
					true_byspec_path + "\\" + file_partner, args, n_value)
		else:
			if file.startswith(args.first_prefix):
				file_partner = find_matching_file(file, file_list, args, n_value)
				compare_one_pair(true_byspec_path + "\\" + file,
					true_byspec_path + "\\" + file_partner, args, n_value)

def print_greatest_diff_in_files(args, n_value):
	true_output_path = args.output_path_for_comparisons + "\\" + (n_value if n_value != '7' else "pwiz_pico")
	file_list = os.listdir(true_output_path)
	with open(true_output_path + "\\file_level_summary.csv", 'w', newline='') as csvfile:
		writer = csv.writer(csvfile)
		if n_value == "2" or n_value == "6":
			writer.writerow(["filename:", "native id of scan with biggest y-diff", "x/ycoordinate of greatest diff", "difference in x at point", "difference in y at point"])
		else:
			writer.writerow(["filename:", "native id of scan with biggest x-diff", "x/ycoordinate of greatest diff", "difference in x at point", "difference in y at point"])
		for file in file_list:
			if file.startswith("XY_SUMMARY"):
				with open(true_output_path + "\\" + file) as f:
					reader = csv.reader(f)
					for row, line_contents in enumerate(reader):
						if row == 2:
							writer.writerow([file,
								line_contents[0] if line_contents[0] else "NULL",
								line_contents[1],
								line_contents[2],
								line_contents[3]])
							break
		writer.writerow([""])
		writer.writerow(["Meta-data differences:"])
		for file in file_list:
			if file.startswith("META_DATA_SUMMARY"):
				with open(true_output_path + "\\" + file) as f:
					reader = csv.reader(f)
					writer.writerow([file + ":"])
					for row, line_contents in enumerate(reader):
						if row == 0:
							continue
						if (line_contents):
							writer.writerow(["",
							line_contents[0] if line_contents[0] else "NULL",
							line_contents[1] if line_contents[1] else "NULL",
							line_contents[2] if line_contents[2] else "NULL"])
						if row == 9:
							break