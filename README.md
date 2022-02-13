
1. Run `git commit`
2. Run `git cat-file commit HEAD`
3. Run `git reset HEAD~1 --soft`. 
4. Modify the `catfile_text` and `commit_msg` in `main()` , also modify the hash condition `if(unlikely(*(uint16_t *)hashbuf == 0))`. Also search for `+0800` if you need to modify timezone. 
5. Commit the result. (You should commit exactly the same changes)


