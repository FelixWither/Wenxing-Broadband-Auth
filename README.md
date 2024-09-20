# Wenxing-Broadband-Auth

自动登录文星宽带，适用于使用了科服2024年新增加的宽带认证系统的宿舍区域

# 1. 作用

科服在2024年下学期给宿舍区宽带加上了**账号密码验证**和设备数量限制。这个程序就是为了**自动化登录流程**而写的。全部基于C语言实现，**适合在资源有限的软路由上自动运行**。

# 2. 用法

##### `注意：`如果你的宽带还没破解设备数量限制（3台），以下过程只能用于登录你当前操作的设备！

## 2.1 基础

#### 2.1.1 登录

1. 先打开宽带登录页面，页面标题应当是 `“慧湖通”网络融合服务门户`，输入账号和密码，点击登录

2. 等待网页自动跳转到运营商选择页面之后，记录下当前网页的网址。应当看起来像是`https://broadband.215123.cn/sso/broadband.html?client=xxxxxxxxx&redirect=http://10.10.16.101:8080/eportal/login_sso.jsp%3Fwlanuserip=xxxxxxxxx&wlanacname=xxxxxxxxx&ssid=&nasip=xxxxxxxxx&snmpagentip=&mac=xxxxxxxxx=wireless-v2&url=xxxxxxxxx&apmac=&nasid=xxxxxxxxx&vid=xxxxxxxxx&port=xxxxxxxxx&nasportid=xxxxxxxxx` 这样

3. 将`redirect=`这里 **等于号之后**的东西全部复制下来

4. 用**记事本**打开`config.yaml`，在`oauth_url:`这一行的**冒号之后**将你复制的内容粘贴上去

5. 在`phone:`和`uid:`这两行分别填上你**登录**宽带**所需**的**手机号**和**身份识别码（身份证号后8位）**。`isp：`处填写运营商名称，中国移动为`chinaMobile`，中国联通为`chinaUnicom`，中国电信为`chinaTelecom`

6. **保存**，双击`auth.exe`测试登录，显示`Success`即为成功

7. **以后**登录直接双击`auth.exe`

#### 2.1.2 登出（下线）

1. 打开`命令行（cmd）`，将这个程序拖进命令行窗口里

2. 输入一个`空格`，随后输入`logout`，回车

## 2.2 命令行操作

#### 2.2.1 支持的操作

- `logout` 登出宽带

#### 2.2.2 支持的参数

- `-c` 用于指定配置文件路径，仅支持`yaml`

#### 2.2.3 命令行格式

- ./auth `<操作>` `[选项]` `<参数>`

- 例子： ./auth `logout` `-c` `/path/to/config.yaml`
