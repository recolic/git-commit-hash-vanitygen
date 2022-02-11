
1. Run `git commit`
2. Run `git cat-file commit HEAD`
3. Modify the `catfile_text` and `commit_msg` in `main()` , also modify the hash condition `if(unlikely(*(uint16_t *)hashbuf == 0))`. Also search for `+0800` if you need to modify timezone. 
4. build and run
5. Run `git reset HEAD~1` and then commit with the result. 


