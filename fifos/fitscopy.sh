if [ $# -lt 1 ]; then
    echo "Syntax: $0 image_name"
fi

image=`echo $1 | tr "/" "\n" | tail -n 1`
exists=`ssh alis ls fits | /bin/grep -c ${image}`
if [ ${exists} -eq 0 ]; then
    scp $1 alis:fits/
    if [ $? -eq 0 ]; then
        ssh alis /home/talon/bin/tidyfits.sh fits/${image}
        rm $1
    fi
fi
