make --always-make PROF=-fprofile-generate win
mobydam.exe -e c:\damend -b none -o B:W22,28,29,31,32,34,36,37,38,39,42,44,48:B2,3,4,6,8,11,12,15,16,19,20,25,35
make --always-make PROF=-fprofile-use win
