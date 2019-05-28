import os, stat, sys, subprocess, copy, argparse, csv
sys.path.insert(0,'src')
import buildExecutables, compareByspecs, generateByspecs

#role of this script is to call the separate parts of original BuildAndCompareByspecsFromTwoMSCompressRepos

proteoWizard_ms_convert = r"C:\Program Files\ProteoWizard\ProteoWizard 3.0.9166\msconvert.exe"
pico_console_exe_location = r"..\ms_compress\Release\pmi_pico_console.exe"
PMi_MSConvert_exe_location = r"E:\dev\builds\qt_common-build\bin\Release"
cmd_for_byspec_extraction = [pico_console_exe_location, "-r", "-t", "1", "-n", "4", "-l", "100", "-d", "0", "-uv", "0.02", "-uf", "c", "-ut", "1000", "-i"]
cmd_for_comparison = [PMi_MSConvert_exe_location + r"\PMi-MSConvert.exe", "--command", "6"]

def populate_arguments():
	parser = argparse.ArgumentParser(description="build .byspec2 files from list of .Raw's, then compare them")

	parser.add_argument('-pp', "--pwiz_pico", action='store_true', help="specifies that the comparisons to be made are between pwize and pico. If this option is specified, the first executable will be ProteoWizard, and the byspecs will be generated in mode 2.\
						You may specify this option in addition to conversion modes")
	parser.add_argument('-fe', "--first_executable", nargs='?', const=r"",
						help="use this flag to specify the location of the first pmi_pico_console.exe to be tested. Specifying this option prevents the program from re-compiling the first branch.")
	parser.add_argument('-se', "--second_executable", nargs='?', const=r"",
						help="use this flag to specify the location of the second pmi_pico_console.exe to be tested. Specifying this option prevents the program from re-compiling the second branch.")
	parser.add_argument ('-dc', "--dont_compare",
						action='store_true',
						help="Include this option to not have the program compare the byspecs it generates. By default, comparisons are made")
	parser.add_argument('-n', "--conversion_modes", nargs='*',
						metavar="conversion_modes",
						help="specifies what mode to run the byspec generation is. Multiple conversion modes may be specified, each separated by a space (e.g. [-n 1 2 5]")
	parser.add_argument('directory_list_file', metavar='directory_list.txt',
						help='Text file with list of directories containing .raw files to check. Format of the file is one filepath per line')
	parser.add_argument('output_path_for_byspecs', metavar="byspec_output_path",
						help='the directory in which to place the .byspec2 files resulting from ms_compress')
	parser.add_argument('output_path_for_comparisons', metavar="output_path_for_comparisons",
						help="the directory in which to place the .csv's resulting from PMi-MSConvert")

	parser.add_argument ('-wf', "--write_full",
						action='store_true',
						help="include to write full x-y diff information to file. Lengthens run-time of program")
	parser.add_argument("-b1", "--first_branch", nargs='?', const=r"develop", default=r"develop",
						metavar='first_ms_compress_branch',
						help="the first branch of ms_compress to be used for comparisons. Defaults to 'develop'. If the --pwiz_pico option is set then this option is ignored")
	parser.add_argument("-b2", "--second_branch", metavar='second_ms_compress_branch',
						nargs='?', const=r"feature/LT-1483-mse-precursor-assignment", default=r"feature/LT-1483-mse-precursor-assignment",
						help="the second branch of ms_compress to be used for comparisons. Defaults to 'feature/LT-1483-mse-precursor-assignment'")
	parser.add_argument("-fp", "--first_prefix", metavar="first_file_prefix",
						nargs='?', const=r"first_", default=r"first_",
						help="Prefix to place before .byspec2 files from first branch. Defaults to 'first_'")
	parser.add_argument("-sp", "--second_prefix", metavar="second_file_prefix",
						nargs='?', const=r"second_", default=r"second_",
						help="Prefix to place before .byspec2 files from second branch. Defaults to 'second_'")
	return parser.parse_args()

if __name__ == '__main__':
	args = populate_arguments()

	with open(args.directory_list_file) as f:
		file_list = f.readlines()

	only_pwiz = False
	if args.pwiz_pico and not args.conversion_modes:
		only_pwiz = True
	if not args.conversion_modes:
		args.conversion_modes = []

	if len(args.conversion_modes) == 0 and not only_pwiz:
		args.conversion_modes += '0'
	if args.pwiz_pico:
		args.conversion_modes += '7'

	if not args.first_executable and not only_pwiz:
		buildExecutables.build_executable(file_list, args, args.first_branch, args.first_prefix)
	if not args.second_executable:
		buildExecutables.build_executable(file_list, args, args.second_branch, args.second_prefix)

	for n_values in args.conversion_modes:
		generateByspecs.generate_byspecs(file_list, args, args.first_prefix, n_values, False if n_values != '7' else True)
		generateByspecs.generate_byspecs(file_list, args, args.second_prefix, n_values, False if n_values != '7' else True)


	for conversion_mode in args.conversion_modes:
		if not args.dont_compare:
			compareByspecs.compare_files(args, conversion_mode)
			compareByspecs.print_greatest_diff_in_files(args, conversion_mode)