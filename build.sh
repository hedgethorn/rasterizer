#!/bin/bash

mkdir -p target

build() {
	gcc -D OS_LINUX -D FREESTANDING src/main_linux.c -o target/main.o -ffreestanding -fno-stack-protector -nostdlib -std=c99 -mavx2 -mfma -c -g -O3 -Wall \
		&& mold target/main.o -o target/main -e begin
}

watch() {
	while true; do
		clear

		echo "Building"

		build

		BUILD_RESULT=$?

		if [ $BUILD_RESULT -eq 0 ]; then
			if [ -n $PID ]; then
				echo "Killing"
				kill $PID 2>/dev/null || true
				wait $PID 2>/dev/null || true
			fi

			echo "Running"
			target/main &
			PID=$!
		fi

		echo "Sleeping"

		inotifywait -q -e modify -r src
	done
}

case "$1" in
	"build") build;;
	"watch") watch;;
	*) build && target/main;;
esac
