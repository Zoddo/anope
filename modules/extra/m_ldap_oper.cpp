#include "module.h"
#include "ldap.h"

static Anope::string opertype_attribute;

class IdentifyInterface : public LDAPInterface
{
	std::map<LDAPQuery, Anope::string> requests;

 public:
	IdentifyInterface(Module *m) : LDAPInterface(m)
	{
	}

	void Add(LDAPQuery id, const Anope::string &nick)
	{
		std::map<LDAPQuery, Anope::string>::iterator it = this->requests.find(id);
		this->requests[id] = nick;
	}

	void OnResult(const LDAPResult &r)
	{
		std::map<LDAPQuery, Anope::string>::iterator it = this->requests.find(r.id);
		if (it == this->requests.end())
			return;
		User *u = finduser(it->second);
		this->requests.erase(it);


		if (!u || !u->Account())
			return;

		try
		{
			const LDAPAttributes &attr = r.get(0);

			const Anope::string &opertype = attr.get(opertype_attribute);

			for (std::list<OperType *>::iterator oit = Config->MyOperTypes.begin(), oit_end = Config->MyOperTypes.end(); oit != oit_end; ++oit)
			{
				OperType *ot = *oit;
				if (ot->GetName() == opertype && ot != u->Account()->ot)
				{
					u->Account()->ot = ot;
					Log() << "m_ldap_oper: Tied " << u->nick << " (" << u->Account()->display << ") to opertype " << ot->GetName();
					break;
				}
			}
		}
		catch (const LDAPException &ex)
		{
			Log() << "m_ldap_oper: " << ex.GetReason();
		}
	}

	void OnError(const LDAPResult &r)
	{
		this->requests.erase(r.id);
	}
};

class LDAPOper : public Module
{
	service_reference<LDAPProvider> ldap;
	IdentifyInterface iinterface;

	Anope::string binddn;
	Anope::string password;
	Anope::string basedn;
	Anope::string filter;
 public:
	LDAPOper(const Anope::string &modname, const Anope::string &creator) :
		Module(modname, creator), ldap("ldap/main"), iinterface(this)
	{
		this->SetAuthor("Anope");
		this->SetType(SUPPORTED);

		Implementation i[] = { I_OnReload, I_OnNickIdentify };
		ModuleManager::Attach(i, this, 2);

		OnReload(false);
	}

	void OnReload(bool)
	{
		ConfigReader config;

		this->binddn = config.ReadValue("m_ldap_oper", "binddn", "", 0);
		this->password = config.ReadValue("m_ldap_oper", "password", "", 0);
		this->basedn = config.ReadValue("m_ldap_oper", "basedn", "", 0);
		this->filter = config.ReadValue("m_ldap_oper", "filter", "", 0);
		opertype_attribute = config.ReadValue("m_ldap_oper", "opertype_attribute", "", 0);
	}

	void OnNickIdentify(User *u)
	{
		try
		{
			if (!this->ldap)
				throw LDAPException("No LDAP interface. Is m_ldap loaded and configured correctly?");
			else if (this->basedn.empty() || this->filter.empty() || opertype_attribute.empty())
				throw LDAPException("Could not search LDAP for opertype settings, invalid configuration.");

			if (!this->binddn.empty())
				this->ldap->Bind(NULL, this->binddn.replace_all_cs("%a", u->Account()->display), this->password.c_str());
			LDAPQuery id = this->ldap->Search(&this->iinterface, this->basedn, this->filter.replace_all_cs("%a", u->Account()->display));
			this->iinterface.Add(id, u->nick);
		}
		catch (const LDAPException &ex)
		{
			Log() << "m_ldapoper: " << ex.GetReason();
		}
	}
};

MODULE_INIT(LDAPOper)