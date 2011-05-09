##
## Darren Chew <darren.chew at vicscouts dot asn dot au>
## Andre Fiebach <andre dot fiebach at stud dot uni-rostock dot de>
## Thomas Mueller 12.04.2003, thomas.mueller@christ-wasser.de
## Richard Renard rrenard@idealx.com 2005-01-28
## - added support for MungedDial, BadPasswordCount, BadPasswordTime, PasswordHistory, LogonHours
## TAKEDA Yasuma yasuma@osstech.co.jp 2008-11-06
## - added sambaTrustedDomainPassword objectClasses
## - in Sun One 5.2 copy it as 99samba-schema-netscapeds5.ldif
##
## Samba 3.2 schema file for Netscape DS 5.x
##
## INSTALL-DIRECTORY/slapd-your_name/config/schema/samba-schema-netscapeds5.ldif
####################################################################
# Sun One DS do not load the schema without this lines
# André Fiebach <af123@uni-rostock.de> 
dn: cn=schema
objectClass: top
objectClass: ldapSubentry
objectClass: subschema
cn: schema
aci: (target="ldap:///cn=schema")(targetattr !="aci")(version 3.0;acl "anonymo
 us, no acis"; allow (read, search, compare) userdn = "ldap:///anyone";)
aci: (targetattr = "*")(version 3.0; acl "Configuration Administrator"; allow 
 (all) userdn = "ldap:///uid=admin,ou=Administrators, ou=TopologyManagement, 
 o=NetscapeRoot";)
aci: (targetattr = "*")(version 3.0; acl "Local Directory Administrators Group
 "; allow (all) groupdn = "ldap:///cn=Directory Administrators, dc=samba,dc=org";)
aci: (targetattr = "*")(version 3.0; acl "SIE Group"; allow (all)groupdn = "ld
 ap:///cn=slapd-sambaldap, cn=iPlanet Directory Server, cn=Server Group, cn=iPlanetDirectory.samba.org, ou=samba.org, o=NetscapeRoot";)
####################################################################
objectClasses: ( 1.3.6.1.4.1.7165.2.2.6 NAME 'sambaSamAccount' SUP top AUXILIARY DESC 'Samba 3.0 Auxilary SAM Account' MUST ( uid $ sambaSID ) MAY  ( cn $ sambaLMPassword $ sambaNTPassword $ sambaPwdLastSet $ sambaLogonTime $ sambaLogoffTime $ sambaKickoffTime $ sambaPwdCanChange $ sambaPwdMustChange $ sambaAcctFlags $ displayName $ sambaHomePath $ sambaHomeDrive $ sambaLogonScript $ sambaProfilePath $ description $ sambaUserWorkstations $ sambaPrimaryGroupSID $ sambaDomainName $ sambaMungedDial $ sambaBadPasswordCount $ sambaBadPasswordTime $ sambaPasswordHistory $ sambaLogonHours) X-ORIGIN 'user defined' )
objectClasses: ( 1.3.6.1.4.1.7165.2.2.4 NAME 'sambaGroupMapping' SUP top AUXILIARY DESC 'Samba Group Mapping' MUST ( gidNumber $ sambaSID $ sambaGroupType ) MAY  ( displayName $ description ) X-ORIGIN 'user defined' )
objectClasses: ( 1.3.6.1.4.1.7165.2.2.5 NAME 'sambaDomain' SUP top STRUCTURAL DESC 'Samba Domain Information' MUST ( sambaDomainName $ sambaSID ) MAY ( sambaNextRid $ sambaNextGroupRid $ sambaNextUserRid $ sambaAlgorithmicRidBase ) X-ORIGIN 'user defined' )
objectClasses: ( 1.3.6.1.4.1.7165.2.2.7 NAME 'sambaUnixIdPool' SUP top AUXILIARY DESC 'Pool for allocating UNIX uids/gids' MUST ( uidNumber $ gidNumber ) X-ORIGIN 'user defined' )
objectClasses: ( 1.3.6.1.4.1.7165.2.2.8 NAME 'sambaIdmapEntry' SUP top AUXILIARY DESC 'Mapping from a SID to an ID' MUST ( sambaSID ) MAY ( uidNumber $ gidNumber )  X-ORIGIN 'user defined' )
objectClasses: ( 1.3.6.1.4.1.7165.2.2.9 NAME 'sambaSidEntry' SUP top STRUCTURAL DESC 'Structural Class for a SID' MUST ( sambaSID )  X-ORIGIN 'user defined' )
objectClasses: ( 1.3.6.1.4.1.7165.2.2.15 NAME 'sambaTrustedDomainPassword' SUP top STRUCTURAL DESC 'Samba Trusted Domain Password' MUST ( sambaDomainName $ sambaSID $ sambaClearTextPassword $ sambaPwdLastSet ) MAY  ( sambaPreviousClearTextPassword ) X-ORIGIN 'user defined')
objectClasses: ( 1.3.6.1.4.1.7165.2.2.16 NAME 'sambaTrustedDomain' SUP top STRUCTURAL DESC 'Samba Trusted Domain Object' MUST ( cn ) MAY ( sambaTrustTyp e $ sambaTrustAttributes $ sambaTrustDirection $ sambaTrustPartner $ sambaFlatName $ sambaTrustAuthOutgoing $ sambaTrustAuthIncoming $ sambaSecurityIdentifier $ sambaTrustForestTrustInfo ) X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.24 NAME 'sambaLMPassword' DESC 'LanManager Password' EQUALITY caseIgnoreIA5Match SYNTAX 1.3.6.1.4.1.1466.115.121.1.26{32} SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.25 NAME 'sambaNTPassword' DESC 'MD4 hash of the unicode password' EQUALITY caseIgnoreIA5Match SYNTAX 1.3.6.1.4.1.1466.115.121.1.26{32} SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.26 NAME 'sambaAcctFlags'	DESC 'Account Flags' EQUALITY caseIgnoreIA5Match SYNTAX 1.3.6.1.4.1.1466.115.121.1.26{16} SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.27 NAME 'sambaPwdLastSet' DESC 'Timestamp of the last password update'	EQUALITY integerMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.28 NAME 'sambaPwdCanChange' DESC 'Timestamp of when the user is allowed to update the password' EQUALITY integerMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.29 NAME 'sambaPwdMustChange' DESC 'Timestamp of when the password will expire' EQUALITY integerMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.30 NAME 'sambaLogonTime'	DESC 'Timestamp of last logon' EQUALITY integerMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.31 NAME 'sambaLogoffTime' DESC 'Timestamp of last logoff' EQUALITY integerMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.32 NAME 'sambaKickoffTime' DESC 'Timestamp of when the user will be logged off automatically' EQUALITY integerMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.33 NAME 'sambaHomeDrive'	DESC 'Driver letter of home directory mapping' EQUALITY caseIgnoreIA5Match SYNTAX 1.3.6.1.4.1.1466.115.121.1.26{4} SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.34 NAME 'sambaLogonScript' DESC 'Logon script path' EQUALITY caseIgnoreMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.15{255} SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.35 NAME 'sambaProfilePath' DESC 'Roaming profile path' EQUALITY caseIgnoreMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.15{255} SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.36 NAME 'sambaUserWorkstations' DESC 'List of user workstations the user is allowed to logon to' EQUALITY caseIgnoreMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.15{255} SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.37 NAME 'sambaHomePath' DESC 'Home directory UNC path' EQUALITY caseIgnoreMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.15{128} )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.38 NAME 'sambaDomainName' DESC 'Windows NT domain to which the user belongs' EQUALITY caseIgnoreMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.15{128} )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.47 NAME 'sambaMungedDial' DESC 'Base64 encoded user parameter string' EQUALITY caseExactMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.15{1050} )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.48 NAME 'sambaBadPasswordCount' DESC 'Bad password attempt count' EQUALITY integerMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 SINGLE-VALUE )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.49 NAME 'sambaBadPasswordTime' DESC 'Time of the last bad password attempt' EQUALITY integerMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 SINGLE-VALUE )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.54 NAME 'sambaPasswordHistory' DESC 'Concatenated MD4 hashes of the unicode passwords used on this account' EQUALITY caseIgnoreIA5Match SYNTAX 1.3.6.1.4.1.1466.115.121.1.26{32} )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.55 NAME 'sambaLogonHours' DESC 'Logon Hours' EQUALITY caseIgnoreIA5Match SYNTAX 1.3.6.1.4.1.1466.115.121.1.26{42} SINGLE-VALUE )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.20 NAME 'sambaSID' DESC 'Security ID' EQUALITY caseIgnoreIA5Match SYNTAX 1.3.6.1.4.1.1466.115.121.1.26{64} SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.23 NAME 'sambaPrimaryGroupSID' DESC 'Primary Group Security ID' EQUALITY caseIgnoreIA5Match SYNTAX 1.3.6.1.4.1.1466.115.121.1.26{64} SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.19 NAME 'sambaGroupType'	DESC 'NT Group Type' EQUALITY integerMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.21 NAME 'sambaNextUserRid' DESC 'Next NT rid to give our for users' EQUALITY integerMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.22 NAME 'sambaNextGroupRid' DESC 'Next NT rid to give out for groups' EQUALITY integerMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.39 NAME 'sambaNextRid' DESC 'Next NT rid to give out for anything' EQUALITY integerMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.40 NAME 'sambaAlgorithmicRidBase' DESC 'Base at which the samba RID generation algorithm should operate' EQUALITY integerMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.68 NAME 'sambaClearTextPassword' DESC 'Clear text password (used for trusted domain passwords)' EQUALITY octetStringMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.40 X-ORIGIN 'user defined')
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.69 NAME 'sambaPreviousClearTextPassword' DESC 'Previous clear text password (used for trusted domain passwords)' EQUALITY octetStringMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.40 X-ORIGIN 'user defined')
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.70 NAME 'sambaTrustType' DESC 'Type of trust' EQUALITY integerMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.71 NAME 'sambaTrustAttributes' DESC 'Trust attributes for a trusted domain' EQUALITY integerMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.72 NAME 'sambaTrustDirection' DESC 'Direction of a trust' EQUALITY integerMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.73 NAME 'sambaTrustPartner' DESC 'Fully qualified name of the domain with which a trust exists' EQUALITY caseIgnoreMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.15{128} X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.74 NAME 'sambaFlatName' DESC 'NetBIOS name of a domain' EQUALITY caseIgnoreMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.15{128} X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.75 NAME 'sambaTrustAuthOutgoing' DESC 'Authentication information for the outgoing portion of a trust' EQUALITY caseExactMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.15{1050} X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.76 NAME 'sambaTrustAuthIncoming' DESC 'Authentication information for the incoming portion of a trust' EQUALITY caseExactMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.15{1050} X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.77 NAME 'sambaSecurityIdentifier' DESC 'SID of a trusted domain' EQUALITY caseIgnoreIA5Match SUBSTR caseExactIA5SubstringsMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.26{64} SINGLE-VALUE X-ORIGIN 'user defined' )
attributeTypes: ( 1.3.6.1.4.1.7165.2.1.78 NAME 'sambaTrustForestTrustInfo' DESC 'Forest trust information for a trusted domain object' EQUALITY caseExactMatch SYNTAX 1.3.6.1.4.1.1466.115.121.1.15{1050} )
