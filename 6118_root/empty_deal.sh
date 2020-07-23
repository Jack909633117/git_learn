if [ "$1" = "add" ]; then
	find . -type d -empty -and -not -regex ./.git.* -exec touch {}/.gitkeep \;
fi
if [ "$1" = "delect" ]; then
	find . -name ".gitkeep" -type f -print -exec rm -rf {} \;
fi

