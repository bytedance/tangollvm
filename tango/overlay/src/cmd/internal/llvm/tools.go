// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 ByteDance Ltd. and/or its affiliates.

//go:build goexperiment.tangollvm && linux && amd64 && cgo && !compiler_bootstrap

package llvm

import (
	"fmt"
	"internal/buildcfg"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"sort"
	"strings"
)

type ToolPath struct {
	projectRoot string
	toolsDir    string
	verbose     int
}

func NewToolPath(toolsRelPath string, verbose int) (*ToolPath, error) {
	projectRoot := runtime.GOROOT()
	if filepath.IsAbs(toolsRelPath) {
		projectRoot = ""
	}
	return &ToolPath{
		projectRoot: projectRoot,
		toolsDir:    toolsRelPath,
		verbose:     verbose,
	}, nil
}

func (p *ToolPath) GetToolPath(toolName string) string {
	if runtime.GOOS == "windows" {
		toolName = toolName + ".exe"
	}

	return filepath.Join(p.projectRoot, p.toolsDir, toolName)
}

type Tools struct {
	*ToolPath
	dir string
}

func NewTools(toolsRelPath string, verbose int) (*Tools, error) {
	toolPath, err := NewToolPath(toolsRelPath, verbose)
	if err != nil {
		return nil, err
	}
	return &Tools{ToolPath: toolPath}, nil
}

func DefaultTools(verbose int) (*Tools, error) {
	root, err := filepath.Abs(tangoLLVMSDKRoot())
	if err != nil {
		return nil, err
	}
	return NewTools(filepath.Join(root, "bin"), verbose)
}

func DefaultLibraryPath() string {
	return filepath.Join(tangoLLVMSDKRoot(), "lib")
}

func (t *Tools) SetDir(dir string) {
	t.dir = dir
}

func (t *Tools) RunShell(exe string, args ...string) error {
	removeEmpty := func(strs []string) []string {
		result := make([]string, 0, len(strs))
		for _, str := range strs {
			if str != "" {
				result = append(result, str)
			}
		}
		return result
	}
	cmd := exec.Command(t.GetToolPath(exe), removeEmpty(args)...)
	if t.dir != "" {
		cmd.Dir = t.dir
	}
	if t.verbose >= 1 {
		fmt.Println("Run: ", exe, strings.Join(cmd.Args, " "))
	}
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	return cmd.Run()
}

func (t *Tools) CompileModule(input string, triple string, features string, eraseDupOk func(string), optArgs []string, args []string) (string, error) {
	output := input + ".o"
	opt := input[:len(input)-3] + ".opt.bc"
	var passes = "-O3"
	for _, v := range optArgs {
		if strings.HasPrefix(v, "-passes") {
			passes = ""
		}
	}

	eraseDupOk(input)

	err := t.RunShell("opt", append(append([]string{
		passes,
		"-mtriple=" + triple,
		"-mattr=" + features,
	}, optArgs...), "-o", opt, input)...)
	if err != nil {
		log.Fatal("opt failed:", err)
	}

	// emit normal result.
	err = t.RunShell("llc", append(append([]string{
		"--filetype=obj",
		"--relocation-model=static",
		"-mattr=" + features,
		"-mtriple=" + triple,
		"-O3",
		"--cost-kind=latency",
	}, args...), "-o", output, opt)...)
	if err != nil {
		log.Fatal("llc failed:", err)
	}

	return output, err
}

// Basic configuration parameters
const (
	ltoPasses       = "-passes=default<O3>,lto<O3>"
	optLevel        = "-O3"
	fileType        = "obj"
	relocationModel = "static"
	costKind        = "latency"
)

// ltoInputs selects only bitcode belonging to this link action. Other main
// packages may be compiling concurrently in the same Go work directory.
func ltoInputs(archives []string) ([]string, error) {
	if len(archives) == 0 {
		return nil, fmt.Errorf("LLVM LTO requires the current target's package archives")
	}
	seen := make(map[string]bool)
	var inputs []string
	for i, archive := range archives {
		input, err := filepath.Abs(strings.TrimSuffix(archive, filepath.Ext(archive)) + "export.bc")
		if err != nil {
			return nil, err
		}
		info, err := os.Stat(input)
		if os.IsNotExist(err) && i != 0 {
			// Packages using the standard backend do not emit LLVM bitcode.
			continue
		}
		if err != nil {
			return nil, fmt.Errorf("LLVM LTO input %q: %w; rebuild with -a", input, err)
		}
		if !info.Mode().IsRegular() {
			return nil, fmt.Errorf("LLVM LTO input is not a regular file: %s", input)
		}
		if !seen[input] {
			seen[input] = true
			inputs = append(inputs, input)
		}
	}
	sort.Strings(inputs)
	return inputs, nil
}

func (t *Tools) RunLTO(archives []string) {
	output, err := t.runLTO(archives)
	if err != nil {
		log.Fatalf("LLVM LTO failed: %v", err)
	}
	os.Setenv("LTO_BIN", output)
}

func (t *Tools) runLTO(archives []string) (string, error) {
	inputs, err := ltoInputs(archives)
	if err != nil {
		return "", err
	}
	triple, err := GetLLVMTriple()
	if err != nil {
		return "", err
	}
	work, err := os.MkdirTemp(t.dir, "tangollvm-lto-")
	if err != nil {
		return "", err
	}
	work, err = filepath.Abs(work)
	if err != nil {
		return "", err
	}
	// Keep this target's intermediates until Go cleans its work directory.
	local := *t
	local.dir = work

	// Join feature array to string
	featureString := GetLLVMFeatures(buildcfg.GOAMD64)

	// opt command optimization parameters
	optParams := []string{
		ltoPasses,
		"-inline-threshold=375",
		"-inlinehint-threshold=475",
		"-inline-cold-callsite-threshold=65",
		"-inlinecold-threshold=65",
		"-hot-callsite-threshold=4500",
		"-locally-hot-callsite-threshold=650",
		"-inline-instr-cost=4",
		"-inline-call-penalty=18",
		"-inline-size-allowance=130",
		"-inline-savings-multiplier=10",
		"-inline-savings-profitable-multiplier=5",
		"-mtriple=" + triple,
		"-mattr=" + featureString,
		"-unified-lto",
	}

	// llc command parameters
	llcParams := []string{
		"--filetype=" + fileType,
		"--relocation-model=" + relocationModel,
		"-mattr=" + featureString,
		"-mtriple=" + triple,
		optLevel,
		"--cost-kind=" + costKind,
		"-tangollvm-lto=true",
	}

	stages := []struct {
		tool string
		args []string
	}{
		{"llvm-link", append(append([]string{}, inputs...), "-v", "-o", "result.bc")},
		{"opt", append(optParams, "-o", "lto.bc", "result.bc")},
		{"llc", append(llcParams, "-o", "lto.o", "lto.bc")},
	}
	for _, stage := range stages {
		if err := local.RunShell(stage.tool, stage.args...); err != nil {
			return "", fmt.Errorf("%s: %w", stage.tool, err)
		}
	}
	output := filepath.Join(work, "lto.o")
	if _, err := os.Stat(output); err != nil {
		return "", err
	}
	return output, nil
}

func (t *Tools) CopyModule(ltoinput string, output string) {
	cmd := exec.Command("cp", ltoinput, output)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	cmd.Run()
}

func (t *Tools) LinkModule(module string, input string, ltoinput string, output string) {
	err := t.RunShell("llvm-link", []string{
		"--skip-when-link",
		"--copy-source",
		"-module-name=" + module,
		input,
		ltoinput,
		"-o",
		output}...)
	if err != nil {
		log.Fatal("opt failed:", err)
	}
}
