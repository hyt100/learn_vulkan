### vulkan环境安装

**安装Vulkan SDK**

参考 [SDK网站](https://vulkan.lunarg.com/) 安装：

```shell
wget -qO - https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo apt-key add -
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-1.2.189-focal.list https://packages.lunarg.com/vulkan/1.2.189/lunarg-vulkan-1.2.189-focal.list
sudo apt update
sudo apt install vulkan-sdk
```

库文件会被安装到：`/usr/lib/x86_64-linux-gnu`

头文件会被安装到：`/usr/include/vulkan`

**安装GLFW**

参考 [官方网站](https://www.glfw.org/) ， 我们使用GLFW来创建窗口。

**Vulkan Samples**

[google samples](https://github.com/googlesamples/vulkan-basic-samples)