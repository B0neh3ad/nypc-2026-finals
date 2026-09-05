# NYPC 2026 Master Track — 개발 도구 사용 안내

> 출처: https://new.nypc.co.kr/ko/rules/master?section=tools
> 내려받은 날짜: 2026-08-29
> **원문이 정본입니다.** 이 파일은 2026-08-29에 내려받은 사본이므로,
> 애매하면 위 원문 주소를 다시 확인하세요.

코드 작성 시 로컬 컴퓨터의 개발 도구를 사용할 수 있습니다. 이 경우 로컬 컴퓨터의 개발 환경과 채점 환경이 달라 실행이나 채점이 되지 않는 불이익을 받지 않도록 본 안내를 꼭 읽어보시기 바랍니다 (특히 Visual Studio / Visual C++ 사용 시).

## 채점 환경

코드 제출 문제에 제출된 모든 코드의 실행과 채점은 다음 환경에서 이루어집니다.

- Amazon Web Services의 c7a.2xlarge 인스턴스
- AMD EPYC 4세대 기반 커스텀 프로세서
- 클럭: 3.7 GHz
- 프로세서 아키텍처: 64 bit
- OS: Ubuntu 24.04

해당 환경에서 동작하지 않는 코드는 채점이 되지 않습니다. 유념하여 개발 도구를 사용해주시기 바랍니다.

### 언어별 컴파일러

#### C20 (`c20`) — gcc 14.2.0

- 사용 가능 외부 라이브러리: (없음)
- 컴파일: `gcc -std=gnu2x -O2 -DONLINE_JUDGE -DNYPC -Wall -Wextra -march=native -mtune=native -o main main.c -lm`
- 실행: `./main NYPC`

#### C++20 (`cpp20`) — gcc 14.2.0

- 사용 가능 외부 라이브러리: Boost 1.90.0
- 컴파일: `g++ -std=gnu++20 -O2 -DONLINE_JUDGE -DNYPC -Wall -Wextra -march=native -mtune=native -o main main.cpp -I/opt/boost/gcc/include -L/opt/boost/gcc/lib`
- 실행: `./main NYPC`

#### Python3 (`python3`) — Python 3.12.3

- 사용 가능 외부 라이브러리: `numpy==2.4.2`, `scipy==1.17.1`, `sortedcontainers==2.4.0`, `more-itertools==10.6.0`, `torch==2.10.0+cpu`, `tensorflow==2.20.0`
- 컴파일: `python3 -m py_compile main.py`
- 실행: `python3 __pycache__/main.cpython-312.pyc NYPC`

#### Pypy3 (`pypy3`) — Python 3.9.18 (Pypy 7.3.15)

- 사용 가능 외부 라이브러리: `sortedcontainers==2.4.0`, `more-itertools==10.6.0`
- 컴파일: `pypy3 -m py_compile main.py`
- 실행: `pypy3 __pycache__/main.pypy39.pyc NYPC`

#### Java 25 (`java25`) — openjdk 25.0.7

- 사용 가능 외부 라이브러리: (없음)
- 컴파일: `javac --release 25 -encoding UTF-8 Main.java -d out && jar cfe Main.jar Main -C out .`
- 실행: `java -Xms<memory> -Xmx<memory> -DONLINE_JUDGE=1 -DNYPC=1 -jar Main.jar NYPC`

#### Rust 2024 (`rust2024`) — rustc 1.93.1

- 사용 가능 외부 라이브러리: `bitvec@1.0.1`, `itertools@0.14.0`, `num@0.4.3`, `rand@0.9.0`
- 컴파일: `cargo rustc -r -q --frozen -- -Copt-level=3 -Ctarget-cpu=native --cfg online_judge --cfg nypc`
- 실행: `./target/release/main NYPC`

#### Node.js (`nodejs24`) — Node.js v24.13.1

- 사용 가능 외부 라이브러리: (없음)
- 컴파일: (컴파일하지 않음)
- 실행: `node main.js NYPC`

#### Typescript 5.9.3 (`nodets24`) — Typescript 5.9.3 (Node.js v24.13.1)

- 사용 가능 외부 라이브러리: `@types/node@24.1.0`
- 컴파일: `tsc main.ts --target ESNext --moduleResolution node --module commonjs --noEmitOnError`
- 실행: `node main.js NYPC`

#### C# 10 (`csharp10`) — dotnet 10.0.103

- 사용 가능 외부 라이브러리: MathNet.Numerics 5.0.0
- 컴파일: `dotnet publish -r linux-x64 -p:PublishSingleFile=true -p:DefineConstants="ONLINE_JUDGE;NYPC" --no-restore -c Release --sc true`
- 실행: `./bin/Release/net10.0/linux-x64/publish/Main NYPC`

#### Go (`golang126`) — go 1.26.0

- 사용 가능 외부 라이브러리: `gonum.org/v1/gonum@v0.16.0`, `github.com/emirpasic/gods@v1.18.1`, `github.com/liyue201/gostl@v1.2.0`
- 컴파일: `go build -mod vendor -o main main.go`
- 실행: `./main NYPC`

#### LuaJIT (`luajit`) — Lua 5.1 (LuaJIT 2.1.1748459687)

- 사용 가능 외부 라이브러리: (없음)
- 컴파일: `luajit -O3 -b Main.lua Main.out`
- 실행: `/usr/local/bin/luajit Main.out NYPC`

#### Lua 5.4 (`lua`) — Lua 5.4.6

- 사용 가능 외부 라이브러리: (없음)
- 컴파일: `/usr/bin/luac5.4 -o Main.out Main.lua`
- 실행: `lua5.4 Main.out NYPC`

#### Kotlin JVM 2.3 (`kotlinjvm`) — kotlinc-jvm 2.3.10

- 사용 가능 외부 라이브러리: (없음)
- 컴파일: `kotlinc -include-runtime -d Main.jar Main.kt`
- 실행: `java -Xms<memory> -Xmx<memory> -DONLINE_JUDGE=1 -DNYPC=1 -jar Main.jar NYPC`

#### Scala JVM 3.8 (`scala3jvm`) — scala 3.8.1

- 사용 가능 외부 라이브러리: (없음)
- 컴파일: `scalac -d Main.jar Main.scala`
- 실행: `java -Xms<memory> -Xmx<memory> -DONLINE_JUDGE=1 -DNYPC=1 -cp /root/.sdkman/candidates/scala/3.8.1/lib/scala/jar:Main.jar Main NYPC`

#### C++26 (`cpp-exp`) — gcc 15

- 사용 가능 외부 라이브러리: Boost 1.90.0
- 컴파일: `g++-15 -std=gnu++26 -O2 -DONLINE_JUDGE -DNYPC -Wall -Wextra -march=native -mtune=native -o main main.cpp -I/opt/boost/gcc/include -L/opt/boost/gcc/lib`
- 실행: `./main NYPC`

#### Rust 2024 Nightly (`rust-exp`) — rustc >=1.95.0-nightly

- 사용 가능 외부 라이브러리: `bitvec@1.0.1`, `itertools@0.14.0`, `num@0.4.3`, `rand@0.9.0`
- 컴파일: `cargo rustc -r -q --frozen -- -Copt-level=3 -Ctarget-cpu=native --cfg online_judge --cfg nypc`
- 실행: `./target/release/main NYPC`

> C++26과 Rust 2024 Nightly는 실험용 언어로 채점 환경 및 컴파일 환경이 수시로 변경될 수 있으며, 정상적인 작동을 보장하지 않습니다.

### 유의 사항

아래의 예시처럼 특정 플랫폼(특히, Windows나 Visual C++)에서만 동작하는 코드를 작성하지 않도록 유의하시기 바랍니다.

| 사용금지 예시 | 비고 |
| --- | --- |
| `void main()` | 표준에 따라 `int main`이어야 함 |
| `getch()` | 대신 `getchar()`를 사용 |
| `fflush(stdin)` | 비표준 함수 사용 |
| `GetTickCount()` | Windows API는 사용할 수 없음 |
| `CString` | `CString`은 표준이 아니며 사용할 수 없음. 대신 `std::string` 을 사용 |
| `#include <stdafx.h>` | `stdafx.h`는 Visual C++ 전용 precompiled header임 |

### Java 유의 사항

클래스 이름은 반드시 `Main` 이어야 합니다.

## 표준 입출력 사용하기

모든 코드 제출 문제에서는 주어진 입력 형식에 따라 표준 입력 (standard input)으로 입력을 받고 주어진 출력 형식에 따라 표준 출력(standard output)으로 출력해야 합니다. 아래 예시와 같이 두 개의 정수를 받아 곱하여 출력해야 하는 문제가 있다면, 언어별로 표준 입력과 표준 출력을 처리하는 방법은 아래와 같습니다.

### 입력 예시

```
8 4
```

### 출력 예시

```
32
```

### 언어별 코드 예시

**C20**

```c
#include <stdio.h>

int main()
{
	int a, b;
	scanf("%d %d", &a, &b);
	printf("%d\n", a * b);
	return 0;
}
```

**C++20**

```cpp
#include <iostream>

using namespace std;

int main()
{
	int a, b;
	cin >> a >> b;
	cout << a * b << endl;
	return 0;
}
```

**Python3**

```python
a, b = map(int, input().split())
print(a * b)
```

**Pypy3**

```python
a, b = map(int, input().split())
print(a * b)
```

**Java 25**

```java
import java.util.Scanner;
public class Main {
    public static void main(String args[]) {
        Scanner sc = new Scanner(System.in);
        int a = sc.nextInt();
        int b = sc.nextInt();
        System.out.println(a * b);
    }
}
```

**Rust 2024**

```rust
use std::io;
fn main() {
    let mut input = String::new();
    io::stdin().read_line(&mut input).unwrap();
    let nums: Vec<i32> = input.trim().split_whitespace()
        .map(|x| x.parse().unwrap())
        .collect();
    println!("{}", nums[0] * nums[1]);
}
```

**Node.js**

```javascript
const input = require('fs').readFileSync('/dev/stdin', 'utf8');
const [a, b] = input.split(' ').map(Number);
console.log(a * b);
```

**Typescript 5.9.3**

```typescript
const input = require('fs').readFileSync('/dev/stdin', 'utf8');
const [a, b] = input.split(' ').map(Number);
console.log(a * b);
```

**C# 10**

```csharp
using System;
class Program {
    static void Main() {
        string[] input = Console.ReadLine().Split(' ');
        int a = int.Parse(input[0]);
        int b = int.Parse(input[1]);
        Console.WriteLine(a * b);
    }
}
```

**Go**

```go
package main
import (
    "fmt"
)
func main() {
    var a, b int
    fmt.Scanf("%d %d", &a, &b)
    fmt.Println(a * b)
}
```

**LuaJIT**

```lua
a, b = io.read("*n", "*n")
print(a * b)
```

**Lua 5.4**

```lua
a, b = io.read("*n", "*n")
print(a * b)
```

**Kotlin JVM 2.3**

```kotlin
fun main() {
    val (a, b) = readLine()!!.split(" ").map { it.toInt() }
    println(a * b)
}
```

**Scala JVM 3.8**

```scala
import scala.io.StdIn.readLine

@main def run(): Unit =
  val nums = readLine().split(" ").map(_.toInt)
  println(nums(0) * nums(1))
```

### 바이너리 파일

채점 환경에서는 소스코드 이외에 바이너리 파일을 추가로 제출할 수 있습니다. 바이너리 파일은 소스코드를 실행하는 디렉토리에 `data.bin`이라는 이름의 파일로 제공됩니다. 해당 파일은 코드 실행 시에만 제공되며, 컴파일 시에는 제공되지 않습니다. `data.bin`의 모든 바이트를 표준 출력으로 출력하는 코드는 다음과 같습니다.

**C20**

```c
#include <stdio.h>

int main()
{
    FILE *f = fopen("data.bin", "rb");
    int byte;
    while ((byte = fgetc(f)) != EOF)
    {
        printf("%d ", byte);
    }
    fclose(f);
    printf("\n");
}
```

**C++20**

```cpp
#include <fstream>
#include <iostream>

using namespace std;

int main()
{
    ifstream file("data.bin", ios::binary);

    unsigned char byte;
    while (file.read((char *)&byte, 1))
    {
        cout << (int)byte << " ";
    }
    cout << endl;

    file.close();
    return 0;
}
```

**Python3**

```python
with open("data.bin", "rb") as f:
    bytes_read = f.read()
    for byte in bytes_read:
        print(byte, end=" ")
    print()
```

**Pypy3**

```python
with open("data.bin", "rb") as f:
    bytes_read = f.read()
    for byte in bytes_read:
        print(byte, end=" ")
    print()
```

**Java 25**

```java
import java.io.FileInputStream;
import java.io.IOException;

public class Main {
    public static void main(String[] args) {
        try (FileInputStream fis = new FileInputStream("data.bin")) {
            int byteRead;
            while ((byteRead = fis.read()) != -1) {
                System.out.print(byteRead + " ");
            }
            System.out.println();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
```

**Rust 2024**

```rust
use std::{fs::File, io::Read};

fn main() {
    let mut file = File::open("data.bin").unwrap();
    let mut buffer = [0u8; 1];
    while let Ok(n) = file.read(&mut buffer) {
        if n == 0 { break; }
        print!("{} ", buffer[0]);
    }
    println!();
}
```

**Node.js**

```javascript
const fs = require("fs");

fs.readFile("data.bin", (_, data) => {
  data.forEach(byte => process.stdout.write(byte + " "));
  console.log();
});
```

**Typescript 5.9.3**

```typescript
const fs = require("fs");

fs.readFile("data.bin", (_, data) => {
  data.forEach(byte => process.stdout.write(byte + " "));
  console.log();
});
```

**C# 10**

```csharp
using System;
using System.IO;

class Program {
    static void Main() {
        using (FileStream fs = new FileStream("data.bin", FileMode.Open, FileAccess.Read)) {
            int byteRead;
            while ((byteRead = fs.ReadByte()) != -1) {
                Console.Write(byteRead + " ");
            }
            Console.WriteLine();
        }
    }
}
```

**Go**

```go
package main

import (
    "fmt"
    "os"
)

func main() {
    file, _ := os.Open("data.bin")
    defer file.Close()

    buf := make([]byte, 1)
    for {
        n, _ := file.Read(buf)
        if n == 0 { break }
        fmt.Printf("%d ", buf[0])
    }
    fmt.Println()
}
```

**LuaJIT**

```lua
local file = assert(io.open("data.bin", "rb"))

while true do
  local byte = file:read(1)
  if not byte then break end
  io.write(string.byte(byte), " ")
end

file:close()
print()
```

**Lua 5.4**

```lua
local file = assert(io.open("data.bin", "rb"))

while true do
  local byte = file:read(1)
  if not byte then break end
  io.write(string.byte(byte), " ")
end

file:close()
print()
```

**Kotlin JVM 2.3**

```kotlin
import java.io.File

fun main() {
    val bytes = File("data.bin").readBytes()
    for (b in bytes) {
        print("${b.toInt() and 0xFF} ")
    }
    println()
}
```

**Scala JVM 3.8**

```scala
import java.nio.file.{Files, Paths}

@main def run(): Unit =
  val bytes = Files.readAllBytes(Paths.get("data.bin"))
  println(bytes.map(_ & 0xFF).mkString(" "))
```

## 프로그램의 종료 코드 (exit code)

작성된 프로그램은 프로그램의 종료 코드가 항상 0 (정상 종료)이 되어야 합니다. 0으로 종료되지 않는 경우 맞는 답을 출력하는 프로그램을 제출하였더라도 채점이 되지 않을 수 있습니다. 특히 종료 코드를 0으로 하는 코드를 제출하였더라도 작성한 프로그램의 런타임 에러 여부에 따라 0이 아닌 코드로 종료될 수 있습니다.
