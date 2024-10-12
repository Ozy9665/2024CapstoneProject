#pragma once

/*--------------
	IocpObject
----------------*/

class IocpObject : public enable_shared_from_this<IocpObject>
{
public:
	virtual ~IocpObject(){}

	virtual HANDLE GetHandle() abstract;
	virtual void Dispatch(struct IocpEvent* iocpEvent, int32 numOfByts) abstract;
};

/*--------------
	IocpCore
----------------*/

class IocpCore
{
public:
	IocpCore();
	~IocpCore();

	HANDLE		GetHandle() {}

	bool		Register(IocpObjectRef iocpObject);
	bool		Dispatch(uint32 timeoutMs = INFINITE);

private:
	HANDLE		_iocpHandle;
};