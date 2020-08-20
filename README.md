# mandelbrot_win32
Mandelbrot explorer for native Windows (32 or 64-bit), not using any runtime libraries!

Build instructions:
* Launch the Visual Studio native tools command prompt ("x86 native tools command prompt"
or "x64 native tools command prompt").
* Navigate to the project directory.
* Run `build.bat`.

The resulting executable should be super tiny (around 10 KB) and should perform with
blazing speed.

You may also open the solution `.sln` in Visual Studio and build it from there, but this
will make it include the runtime, which will make the executable around 70 KB.

----

Copyright (c) 2020 Dmitry Brant.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
