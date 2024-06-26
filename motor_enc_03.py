import RPi.GPIO as gpio
import time
from math import pi

gpio.setmode(gpio.BCM)

fwd_flag = True

circ = 2 * pi * 5

output_pins={}
output_pins['dir1']=26
output_pins['dir2']=24
output_pins['pwm1']=12
output_pins['pwm2']=13

m1_A = 25
m1_B = 23
m2_A = 10
m2_B = 9

for i in output_pins.keys():
    pin = output_pins[i];
    print(pin)
    gpio.setup(pin,gpio.OUT);


pwm1 = gpio.PWM(output_pins['pwm1'],100)
pwm2 = gpio.PWM(output_pins['pwm2'],100)

gpio.setup(m1_A,gpio.IN,pull_up_down = gpio.PUD_DOWN)
gpio.setup(m1_B,gpio.IN,pull_up_down = gpio.PUD_DOWN)
gpio.setup(m2_A,gpio.IN,pull_up_down = gpio.PUD_DOWN)
gpio.setup(m2_B,gpio.IN,pull_up_down = gpio.PUD_DOWN)


counter_dict={}
def reset_counter():
    counter_dict[1]=0;
    counter_dict[2]=0;

rotation={}
rotation[1]=0
rotation[2]=0

def increment1(pin):
    counter_dict[1] +=1
    if(counter_dict[1]==3316):
        rotation[1]+=1
        counter_dict[1]=0

def increment2(pin):
    counter_dict[2] +=1;
    if(counter_dict[2] == 3316):
        rotation[2]+=1
        counter_dict[2]=0

gpio.add_event_detect(m1_A,gpio.BOTH,callback=increment1)
gpio.add_event_detect(m1_B,gpio.BOTH,callback=increment1)
gpio.add_event_detect(m2_A,gpio.BOTH,callback=increment2)
gpio.add_event_detect(m2_B,gpio.BOTH,callback=increment2)


def move(speed):
    gpio.output(output_pins['dir1'],gpio.HIGH)
    gpio.output(output_pins['dir2'],gpio.HIGH)
    pwm1.start(speed)
    pwm2.start(speed)

def correct_diff(motor,clicks):
    stop()
    time.sleep(0.2)
    if(motor == 1):
        dir_pin = output_pins['dir1'];
        pwm = pwm1
    else:
        dir_pin = output_pins['dir2'];
        pwm = pwm2
    while(counter_dict[motor] < clicks):
        gpio.output(dir_pin,gpio.HIGH)
        pwm.start(20);
    stop()


def stop():
    pwm1.stop()
    pwm2.stop()

reset_counter()

move(40)

print('W1_Rot\tW2_Rot\tW1_Cl\tW2_Cl\tDist1\tDist2\tDiff\t%Err')

for i in range(0,10):
    time.sleep(1)
    dist1 = circ * (rotation[1] + (counter_dict[1]/3316));
    dist2 = circ * (rotation[2] + (counter_dict[2]/3316));
    diff = dist1 - dist2
    pce = 100* diff / dist1
    print(f'{rotation[1]:.3f}\t{rotation[2]:.3f}\t{counter_dict[1]}\t{counter_dict[2]}\t{dist1:.3f}\t{dist2:.3f}\t{diff:.3f}\t{pce:.3f}')

stop()

time.sleep(2)

tot_click1 = (rotation[1] * 3316) + counter_dict[1]
tot_click2 = (rotation[2] * 3316) + counter_dict[2]

diff_click = tot_click1 - tot_click2

if(diff_click < 0):
    # motor 2 has moved more than motor 1 - move motor1 to adjust
    reset_counter()
    motor = 2
    print(f'motor ID: {motor}\tDifference: {diff_click}')
    time.sleep(1)
    correct_diff(motor,abs(diff_click))
else:
    reset_counter()
    motor = 1
    print(f'motor ID: {motor}\tDifference: {diff_click}')
    time.sleep(1)
    correct_diff(motor,abs(diff_click))


stop()
gpio.cleanup()
