REM Deduce revision number from commit count as 4000 plus commit count
for /f %%x in ('git rev-list --count master') do set number=%%x
set number=000000000%number%
echo #define BUILD_GIT_REV 4%number:~-3%
REM Identify last commit date
for /f "DELIMS=" %%x in ('git log -1 --format^="%%cd" --date^=format:"%%b %%d, %%Y"') do (
	echo #define BUILD_GIT_DATE "%%x"
)
REM Parse stuff from FETCH_HEAD file
for /f "TOKENS=1,3,5" %%x in (%~1) do (
	echo #define BUILD_GIT_SHA "%%x"
	echo #define BUILD_GIT_BRANCH "%%y"
	echo #define BUILD_GIT_URL "%%z"
)
