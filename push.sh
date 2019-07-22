#!/usr/bin/bash
sudo git pull
sudo git add .
expect <<!
    set timeout 20
    spawn sudo git push
    expect {
          "Username" { 
                send "873222366@qq.com\n" 
                exp_continue
                }
          "Password" { 
                send "951025xlh\n" 
                exp_continue
                }
interact
}
!
