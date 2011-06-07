copy jlibfli\Release\jlibfli.dll
javac JLibFLI.java
javac JLibFLITest.java

javah -jni JLibFLI
copy JLibFLI.h jlibfli

