# Secure Aggregation Protocol based on Intel SGX
This is my final year project, support secure aggragtion and TopBinary network compression that combines Top-$k$ sparsification and 1-bit quantization.
## Prerequisites
* Make sure Intel SGX is installed, please refer to the website 
```
https://github.com/intel/linux-sgx
```
* Download the source code
```
git clone https://github.com/55199789/FYP.git
```
## Compile and Run
* Enter the following commands
```
cd FYP & make
./app <clientNum> <vectorDim> [compressionRatio]
```
* To run in simulation mode
```
cd FYP & make SGX_MODE=SIM
./app <clientNum> <vectorDim> [compressionRatio]
```
