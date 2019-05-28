set encoding=utf-8
set shiftwidth=4
set softtabstop=4
set tabstop=8
set tags=tags;/,*.tags;/
set textwidth=78

map <C-F11> :!ctags -R --c++-kinds=+p --fields=+iaS --extra=+q .<CR>
