<img width="1024" height="559" alt="image" src="https://github.com/user-attachments/assets/a3d9cdc0-9aee-4d2c-a173-7c7ee0b41d83" />

<div align="center">
  <img width="45%" alt="image1" src="https://github.com/user-attachments/assets/d9cdfa61-ca6b-4f2e-b367-0971c97a845f" />
  <img width="45%" alt="image2" src="https://github.com/user-attachments/assets/8ea76e89-d73f-4170-9469-019460fbb71d" />
</div>
<img width="1024" height="559" alt="image" src="https://github.com/user-attachments/assets/c00d229f-a4cb-4623-be08-18cdf324367d" />

# schedule：

[娲石介绍文档](https://docs.google.com/document/d/126ceDcdRahR22ykr595SQH-28MgJhu0QrfvUimQ3FZw/edit?usp=sharing)

## 1. only uart wire verision for agent structure design and debug.
- 1.1 audio FPGA project test case.
- 1.2 ascend 310 test case.
## 2. ESP32JTAG+FPGA wireless version.
- 2.1 repalce fpga with GW serials.
- 2.2 portting and debug esp32.
- 2.3 re-construct verilog code for GW serials.
- 2.4 add logic data recovery(SPDIF/IIS/IIC), WaStone only transfer datas to PC web, PC support python plugin to parser datas(could driven by AI agent);
## 3. WaStone with arm version.
- 3.1 select arm hardware;
- 3.2 opencv get position;
- 3.3 push reset key and insert usb(magnetic suction) port; 
# AEL — AI-Driven Embedded Engineering

## License

WaStone is released under the [Apache 2.0 License](https://choosealicense.com/licenses/apache-2.0/).

You are free to:

- use it in personal projects
- integrate it into commercial products
- extend it for internal tooling

Third-party components and vendor code remain under their respective original licenses.
