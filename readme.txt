USBCANFD��������libusbʵ�֣���ȷ�����л�������libusb-1.0�Ŀ⡣
�����ubuntu�����������߰�װ���������£�
# apt-get install libusb-1.0-0

��libusbcanfd.so����/libĿ¼��

����testĿ¼�������������в��Գ��򣬻��ӡCAN���Բ���˵����
# ./test

�������������test���ɽ����շ����ԣ�
# ./test 33 0 3 2 0 10 1000 0

CAN������ĵ���demo��test.c�У��ɲο����ж��ο�����

�豸���Գ������

1���鿴ϵͳ�Ƿ�����ö�ٵ�usb�豸����ӡ���ǵ�VID/PID��USBCANFDΪ04cc:1240����
	# lsusb

2���鿴ϵͳ������USB�豸�ڵ㼰�����Ȩ�ޣ�
	# ls /dev/bus/usb/ -lR

3���޸�usb�豸�ķ���Ȩ��ʹ��ͨ�û����Բ���������xxx��Ӧlsusb�����Ϣ�е�bus��ţ�yyy��Ӧdevice��ţ�
	# chmod 666 /dev/bus/usb/xxx/yyy

4�����Ҫ���ø�����ͨ�û�����USBCANFD�豸��Ȩ�ޣ���Ҫ�޸�udev���ã������ļ���/etc/udev/rules.d/50-usbcanfd.rules���������£�
	SUBSYSTEMS=="usb", ATTRS{idVendor}=="04cc", ATTRS{idProduct}=="1240", GROUP="users", MODE="0666"

	���¼���udev��������豸����Ӧ����Ȩ�ޣ�
	# udevadm control --reload
