import sys
# keys to move a debug numeric input from `start` to `target` (starting on digit 0)
def keys(start, target):
    out=[]; delta=target-start; digit=0
    digits=[]
    d=abs(delta); p=0
    while d:
        digits.append(d%10); d//=10
    key='UP' if delta>0 else 'DOWN'
    for i,n in enumerate(digits):
        if i>0: out+=['press RIGHT','wait 3']
        out+=[f'press {key}','wait 3']*n
    out+=['press LEFT','wait 3']*max(0,len(digits)-1)
    return out
if __name__=='__main__':
    print(';'.join(keys(int(sys.argv[1]),int(sys.argv[2]))))
