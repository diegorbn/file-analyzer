.PHONY: all demo benchmark plugins test clean help
all: demo benchmark
demo:
	$(MAKE) -C demo all
benchmark:
	$(MAKE) -C benchmark all
plugins:
	$(MAKE) -C demo plugins
	$(MAKE) -C benchmark plugins
test: all
	python3 tests/test_project.py
clean:
	$(MAKE) -C demo clean
	$(MAKE) -C benchmark clean

help:
	@printf '%s\n' 'make all        Build both applications and the runner' 'make demo       Build demo applications and plugins' 'make benchmark  Build benchmark applications and plugins' 'make plugins    Build staged plugins' 'make test       Build and run tests' 'make clean      Remove compiled outputs (preserves data and results)'
