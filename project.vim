set path=,,src/**,test/**,include/**

" build
set makeprg=node\ make.js
nnoremap <Leader>b :!node make.js<CR>
