set path=,,src/**,test/**,include/**

" build
set makeprg=node\ make.js
nnoremap <Leader>b :!node make.js<CR>

augroup msgstream
	autocmd!
	autocmd BufNewFile *.c,*.h,*.cpp,*.hpp :0r <sfile>:h/vim/template/skeleton.c
augroup END
