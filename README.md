#FlicTool [![Build Status](https://travis-ci.org/Merigrim/FlicTool.svg?branch=master)](https://travis-ci.org/Merigrim/FlicTool)

FlicTool is a LEGO&reg; Rock Raiders Flic Animation compiler/decompiler.

##Usage

```shell
FlicTool input [output]
```

FlicTool will automatically figure out whether you are looking to decompile an FLH file or compile a folder of frames based on what you supply as `input`.

`output` is optional, not specifying it will make it default to `output.flh` when compiling and `output` when decompiling.

##Notes

At the moment, FlicTool only supports the exact FLH format used by LEGO&reg; Rock Raiders. Furthermore, when compiling individual frames into a new FLH file, only bitmaps with a depth of 16 bits are supported. Bit depth downsampling will most likely be implemented in a future version, but to make sure that the colors stay consistent, I recommend only working with 16-bit files.

##Dependencies

FlicTool requires Boost Filesystem, Program Option, Regex and System in order to be built.
