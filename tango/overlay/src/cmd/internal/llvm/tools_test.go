// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 ByteDance Ltd. and/or its affiliates.

//go:build goexperiment.tangollvm && linux && amd64 && cgo && !compiler_bootstrap

package llvm

import (
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
)

func TestInitUsesSelectedSDK(t *testing.T) {
	root := t.TempDir()
	t.Setenv("TANGOLLVM_SDK_ROOT", root)
	libdir := filepath.Join(root, "lib")
	if err := os.Mkdir(libdir, 0755); err != nil {
		t.Fatal(err)
	}
	source := filepath.Join(root, "empty-env.c")
	if err := os.WriteFile(source, []byte("void *TangoLLVMCreateTangoLLVMEnvironment(void) { return 0; }\n"), 0644); err != nil {
		t.Fatal(err)
	}
	lib := filepath.Join(libdir, "libLLVM.so")
	cmd := exec.Command("cc", "-shared", "-fPIC", source, "-o", lib)
	if out, err := cmd.CombinedOutput(); err != nil {
		t.Fatalf("build library fixture: %v\n%s", err, out)
	}
	err := InitTangoLLVMEnv("libLLVM.so")
	if err == nil || !strings.Contains(err.Error(), "cannot create TangoLLVMEnv") || !strings.Contains(err.Error(), lib) {
		t.Fatalf("expected the selected SDK's null environment error, got %v", err)
	}
}

func TestDefaultToolsSDKLocation(t *testing.T) {
	for _, root := range []string{t.TempDir(), "relative-sdk"} {
		t.Run(root, func(t *testing.T) {
			t.Setenv("TANGOLLVM_SDK_ROOT", root)
			tools, err := DefaultTools(0)
			if err != nil {
				t.Fatal(err)
			}
			abs, err := filepath.Abs(root)
			if err != nil {
				t.Fatal(err)
			}
			if got, want := tools.GetToolPath("opt"), filepath.Join(abs, "bin", "opt"); got != want {
				t.Fatalf("opt path = %q, want %q", got, want)
			}
			lib, err := filepath.Abs(DefaultLibraryPath())
			if err != nil {
				t.Fatal(err)
			}
			if want := filepath.Join(abs, "lib"); lib != want {
				t.Fatalf("library path = %q, want %q", lib, want)
			}
		})
	}
}

func TestLTOInputsAreTargetScoped(t *testing.T) {
	root := t.TempDir()
	var archives []string
	for _, name := range []string{"main one", "shared", "other main", "standard"} {
		dir := filepath.Join(root, name)
		if err := os.Mkdir(dir, 0755); err != nil {
			t.Fatal(err)
		}
		archive := filepath.Join(dir, "_pkg_.a")
		if name != "standard" {
			if err := os.WriteFile(filepath.Join(dir, "_pkg_export.bc"), nil, 0644); err != nil {
				t.Fatal(err)
			}
		}
		archives = append(archives, archive)
	}
	inputs, err := ltoInputs([]string{archives[0], archives[1], archives[1], archives[3]})
	if err != nil {
		t.Fatal(err)
	}
	if len(inputs) != 2 || strings.Contains(strings.Join(inputs, "\n"), "other main") {
		t.Fatalf("unexpected LTO inputs: %v", inputs)
	}
	if _, err := ltoInputs([]string{archives[3]}); err == nil || !strings.Contains(err.Error(), "rebuild with -a") {
		t.Fatalf("missing main bitcode should fail clearly: %v", err)
	}
}

func TestLTOWorkIsolationAndFailure(t *testing.T) {
	root := t.TempDir()
	bin := filepath.Join(root, "SDK with ' quotes", "bin")
	if err := os.MkdirAll(bin, 0755); err != nil {
		t.Fatal(err)
	}
	script := `#!/bin/sh
set -eu
tool=${0##*/}
printf '%s\n' "$tool" >> steps
printf '%s\n' "$@" > "$tool.args"
if [ "${TANGO_TEST_FAIL_TOOL:-}" = "$tool" ]; then exit 7; fi
while [ "$#" -gt 0 ]; do
  if [ "$1" = -o ]; then shift; : > "$1"; break; fi
  shift
done
`
	for _, name := range []string{"llvm-link", "opt", "llc"} {
		if err := os.WriteFile(filepath.Join(bin, name), []byte(script), 0755); err != nil {
			t.Fatal(err)
		}
	}
	archive := filepath.Join(root, "main.a")
	if err := os.WriteFile(filepath.Join(root, "mainexport.bc"), nil, 0644); err != nil {
		t.Fatal(err)
	}
	tools, err := NewTools(bin, 0)
	if err != nil {
		t.Fatal(err)
	}
	tools.SetDir(root)
	type result struct {
		path string
		err  error
	}
	done := make(chan result, 2)
	for i := 0; i < 2; i++ {
		go func() {
			path, err := tools.runLTO([]string{archive})
			done <- result{path, err}
		}()
	}
	first, second := <-done, <-done
	if first.err != nil || second.err != nil || first.path == second.path {
		t.Fatalf("concurrent LTO outputs: %+v, %+v", first, second)
	}
	for _, output := range []string{first.path, second.path} {
		steps, err := os.ReadFile(filepath.Join(filepath.Dir(output), "steps"))
		if err != nil || string(steps) != "llvm-link\nopt\nllc\n" {
			t.Fatalf("pipeline steps: %q, %v", steps, err)
		}
	}
	t.Setenv("TANGO_TEST_FAIL_TOOL", "opt")
	failed := filepath.Join(root, "failure")
	if err := os.Mkdir(failed, 0755); err != nil {
		t.Fatal(err)
	}
	tools.SetDir(failed)
	if _, err := tools.runLTO([]string{archive}); err == nil || !strings.Contains(err.Error(), "opt:") {
		t.Fatalf("expected opt failure: %v", err)
	}
	steps, err := filepath.Glob(filepath.Join(failed, "tangollvm-lto-*", "steps"))
	if err != nil || len(steps) != 1 {
		t.Fatalf("failed pipeline paths: %v, %v", steps, err)
	}
	content, err := os.ReadFile(steps[0])
	if err != nil || string(content) != "llvm-link\nopt\n" {
		t.Fatalf("pipeline continued after failure: %q, %v", content, err)
	}
}
