<div class="markdown-google-sans">
<h1><strong>Bidirectional Regex Inference</strong></h1>
</div>

This work is based on the Parallel Regular Expression Synthesiser (**PaRESy**) by *Mojtaba Valizadeh* and *Martin Berger*.  

For more details on their work, refer to their [paper](https://dl.acm.org/doi/10.1145/3591274).

## Introduction

This work aims to explore new approaches for scaling Regex Inference by sacrificing `minimality` while preserving `precision`. The inference process takes as input a set of positive strings, a set of negative strings, and a cost function. It then produces a regular expression that accepts all positive strings, rejects all negative strings, and remains as close to minimal cost as possible.

In this work, a simple grammar have been used for the REs:

```
R ::= Φ|ε|a|R?|R*|R.R|R+R|R&R
```
For minimality, a cost function is defined that assigns a positive integer to each constructor in the regular expression (RE). The total cost of an RE is the sum of its constructors’ costs. This approach helps prevent overfitting and avoids producing the trivial RE that is simply the union of all positive strings.  

## Build

```bash
   cmake -S . -B build-bidirectional -DBUILD_BIDIRECTIONAL=ON
   cmake --build build-bidirectional --config Release
```

```bash
   cmake -S . -B build-bottom-up-cuda -DBUILD_BOTTOM_UP_CUDA=ON
   cmake --build build-bottom-up-cuda --config Release
```