# Windows targets (primary)
.PHONY: build-win run-win clean-win

build-win:
	scripts\build_windows.bat

run-win:
	build\Release\vulkan-engine.exe

clean-win:
	scripts\clean_windows.bat

# Docker targets (alternative)
.PHONY: build-docker run-docker clean-docker distclean-docker

build-docker:
	docker-compose build

run-docker:
	docker-compose run --rm vulkan-engine

clean-docker:
	rm -rf build/

distclean-docker:
	docker-compose down -v
	rm -rf build/

# Defaults to Windows targets
build: build-win
run: run-win
clean: clean-win
distclean: clean-win 