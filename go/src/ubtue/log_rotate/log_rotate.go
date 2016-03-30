// A simple tool for log rotation.
/*
   Copyright (C) 2016, Library of the University of Tübingen

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Affero General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
package main

import "flag"
import "fmt"
import "os"
import "path"
import "path/filepath"
import "strconv"

// Handle command-line arguments.
func processFlags(maxRotationCount *int) {
	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, "usage: %s [--max-rotation-count max_rotations] log_base_names\n", path.Base(os.Args[0]))
		flag.PrintDefaults()
		os.Exit(1)
	}
	localMaxRotationCount := flag.Int("max-rotation-count", 5, "The maximum number of log file rotations.")
	flag.Parse()
	if *localMaxRotationCount < 1 {
		fmt.Fprintf(os.Stderr, "%s: max-rotation-count must be positive!\n", path.Base(os.Args[0]))
		os.Exit(1)
	}
	*maxRotationCount = *localMaxRotationCount
}

// Either return the log file names provided on the command-line, or, if there are none,
// return a list of files matching "*.log".
func getLogNames() []string {
	logNames := flag.Args()
	if len(logNames) == 0 {
		logNames, _ = filepath.Glob("*.log")
	}
	return logNames
}

func Exists(path string) bool {
	_, err := os.Stat(path)
	return err == nil
}

func processName(logFileName string, maxRotationCount int) {
	if !Exists(logFileName) {
		return
	} else {
		os.Remove(logFileName + "." + strconv.Itoa(maxRotationCount))
	}

	for countSuffix := maxRotationCount; countSuffix > 1; countSuffix-- {
		os.Rename(logFileName+"."+strconv.Itoa(countSuffix-1), logFileName+"."+strconv.Itoa(countSuffix))
	}
	os.Rename(logFileName, logFileName+".1")
}

func main() {
	var maxRotationCount int
	processFlags(&maxRotationCount)
	fmt.Printf("maxRotationCount = %d\n", maxRotationCount)
	logNames := getLogNames()
	for _, name := range logNames {
		processName(name, maxRotationCount)
	}
}
