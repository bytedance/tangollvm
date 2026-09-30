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
	"strings"
)

const (
	GO_ABI0_CCONV        = 112
	GO_ABIINTERNAL_CCONV = 113
	CPU_X86_64           = "x86-64"
)

type Config struct {
	// Target and output information.
	Triple          string
	CPU             string
	Features        string
	CodeModel       string
	RelocationModel string
	SizeLevel       int

	Debug bool
}

func DefaultConfig() (*Config, error) {
	triple, err := GetLLVMTriple()
	if err != nil {
		return nil, err
	}
	return &Config{
		Triple:          triple,
		CPU:             CPU_X86_64,
		Features:        GetLLVMFeatures(buildcfg.GOAMD64),
		CodeModel:       "default",
		RelocationModel: "static",
		SizeLevel:       1,
		Debug:           true,
	}, nil
}

// Get the triple used to define LLVM target
func GetLLVMTriple() (string, error) {
	if buildcfg.GOARCH == "amd64" {
		if buildcfg.GOOS == "linux" {
			triple := "x86_64-unknown-linux-goabi"
			return triple, nil
		} else if buildcfg.GOOS == "darwin" {
			triple := "x86_64-apple-darwin-goabi"
			return triple, nil
		} else {
			return "", fmt.Errorf("unsupported OS: %s", buildcfg.GOOS)
		}
	} else {
		return "", fmt.Errorf("unsupported ARCH: %s", buildcfg.GOARCH)
	}
}

// NewTargetMachine returns a new TargetMachine based on the passed-in
// configuration. It is used by the compiler and is needed for machine code
// emission.
func NewTargetMachine(config *Config) (TargetMachine, error) {
	target, err := GetTargetFromTriple(config.Triple)
	if err != nil {
		return TargetMachine{}, err
	}

	var codeModel CodeModel
	var relocationModel RelocMode

	switch config.CodeModel {
	case "default":
		codeModel = CodeModelDefault
	case "tiny":
		codeModel = CodeModelTiny
	case "small":
		codeModel = CodeModelSmall
	case "kernel":
		codeModel = CodeModelKernel
	case "medium":
		codeModel = CodeModelMedium
	case "large":
		codeModel = CodeModelLarge
	}

	switch config.RelocationModel {
	case "static":
		relocationModel = RelocStatic
	case "pic":
		relocationModel = RelocPIC
	case "dynamicnopic":
		relocationModel = RelocDynamicNoPic
	}

	machine := target.CreateTargetMachine(config.Triple, config.CPU, config.Features, CodeGenLevelAggressive, relocationModel, codeModel)
	return machine, nil
}

// Just follow the info on wiki: https://en.wikipedia.org/wiki/X86-64#Microarchitecture_levels.
// Some features are currently commented out due to various reasons.
func GetLLVMFeatures(amd64ver int) string {
	var features []string

	// Add features for v1.
	features = append(features, "+cmov")
	features = append(features, "+cx8")
	// Not found in an example CPU
	//features = append(features, "+fpu")
	features = append(features, "+fxsr")
	features = append(features, "+mmx")
	// Not found in an example CPU
	//features = append(features, "+osfxsr")
	// Not recognized in LLVM
	//features = append(features, "+sce")
	features = append(features, "+sse")
	features = append(features, "+sse2")

	// Add features for v2.
	if buildcfg.GOAMD64 >= 2 && amd64ver >= 2 {
		// Not found in an example CPU
		//features = append(features, "+cmpxchg16b")
		// Not found in an example CPU
		//features = append(features, "+lahf")
		features = append(features, "+popcnt")
		// Not found in an example CPU
		//features = append(features, "+sse3")
		features = append(features, "+sse4.1")
		features = append(features, "+sse4.2")
		features = append(features, "+ssse3")
	}

	// Add features for v3.
	if buildcfg.GOAMD64 >= 3 && amd64ver >= 3 {
		features = append(features, "+avx")
		features = append(features, "+avx2")
		// Not recognized in LLVM
		//features = append(features, "+bmi1")
		features = append(features, "+bmi2")
		features = append(features, "+f16c")
		features = append(features, "+fma")
		// Not found in an example CPU
		//features = append(features, "+lzcnt")
		features = append(features, "+movbe")
		// Not found in an example CPU
		//features = append(features, "+osxsave")
	}

	// Add features for v4.
	if buildcfg.GOAMD64 >= 4 && amd64ver >= 4 {
		features = append(features, "+ermsb")
		// How to deal with FSRM? It's on some v4 CPUs only.
		//features = append(features, "+fsrm")
		// DON'T ENABLE AVX512 YET.
		//features = append(features, "+avx512f")
		//features = append(features, "+avx512bw")
		//features = append(features, "+avx512cd")
		//features = append(features, "+avx512dq")
		//features = append(features, "+avx512vl")
	}

	return strings.Join(features, ",")
}
