set path=,,src/**,test/**,include/**,.github/workflows

" build
set makeprg=node\ make.mjs
nnoremap <Leader>b :!node make.mjs<CR>

augroup msgstream
	autocmd!
	autocmd BufNewFile *.c,*.h,*.cpp,*.hpp :0r <sfile>:h/vim/template/skeleton.c
augroup END
