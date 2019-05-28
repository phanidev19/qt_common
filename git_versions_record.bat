@echo off
setlocal

CD %1
REM compiling with multiple core causes issue with the RAW compilation.
PATH=%PATH%;C:\Program Files (x86)\Git\bin;C:\Program Files\Git\bin

set out_file=%CD%\git_versions_record.txt
echo ^<out_file_path^>%CD%^</out_file_path^> > %out_file%
echo ^<date^> %date% ^</date^>  >> %out_file%
echo ^<time^> %time% ^</time^> >> %out_file%

pushd %CD%
cd ..\byonic_solution
call :do_git_stuff
popd

pushd %CD%
cd ..\qt_apps_libs
call :do_git_stuff
popd

pushd %CD%
cd ..\lcmsfeaturefinder
call :do_git_stuff_label
popd

pushd %CD%
cd ..\modificationsui
call :do_git_stuff
popd

pushd %CD%
cd ..\poi_report
call :do_git_stuff
popd

pushd %CD%
cd ..\pwiz_
call :do_git_stuff
popd

pushd %CD%
cd ..\wiffreadercomwrapper
call :do_git_stuff
popd

pushd %CD%
cd ..\ms_compress
call :do_git_stuff
popd

pushd %CD%
cd ..\pmi_externals_libs
call :do_git_stuff
popd

pushd %CD%
cd ..\qt_license
call :do_git_stuff
popd

pushd %CD%
cd ..\supernovo
call :do_git_stuff
popd

pushd %CD%
cd ..\intact_decon_clean
call :do_git_stuff
popd

endlocal

goto :eof

:do_git_stuff
echo %CD%
echo ^<sec^>-----------------^</sec^> >> %out_file%

echo ^<full_path^>%CD%^</full_path^> >> %out_file%

echo ^<git_describe^> >> %out_file%
git describe --match "v[0-9].*" --tags --long >> %out_file%
echo ^</git_describe^> >> %out_file%

echo ^<git_branch^> >> %out_file%
git rev-parse --abbrev-ref HEAD >> %out_file%
echo ^</git_branch^> >> %out_file%

echo ^<git_commit^> >> %out_file%
git rev-parse HEAD >> %out_file%
echo ^</git_commit^> >> %out_file%

goto :eof


::lcmsfeaturefinder doesn't have tag with prefix 'v'.  So, making a new function specific for such repos
:do_git_stuff_label
echo %CD%
echo ^<sec^>-----------------^</sec^> >> %out_file%

echo ^<full_path^>%CD%^</full_path^> >> %out_file%

echo ^<git_describe^> >> %out_file%
git describe --tags --long >> %out_file%
echo ^</git_describe^> >> %out_file%

echo ^<git_branch^> >> %out_file%
git rev-parse --abbrev-ref HEAD >> %out_file%
echo ^</git_branch^> >> %out_file%

echo ^<git_commit^> >> %out_file%
git rev-parse HEAD >> %out_file%
echo ^</git_commit^> >> %out_file%


goto :eof

