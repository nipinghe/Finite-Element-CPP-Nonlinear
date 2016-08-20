/*
 * FiniteElemNL.cpp
 *
 * This class runs the finite element method and obtains a
 * numerical solution to a PDE on a domain given into the constructor.
 *
 * Currently, only two functions have been implemented, and square domains.
 *
 *  Created on: Aug 13, 2016
 *      Author: Ted Kwan
 */

#include "FiniteElemNL.h"
#include "FunZeros.h"
#include "FunOnes.h"
#include "FunSC.h"
#include "FunSCNL.h"
#include "FunSCbD.h"
#include "FunBoltz.h"
#include <iostream>

using namespace std;
using namespace arma;

FiniteElemNL::FiniteElemNL() {

}

/**
 * Constructor to create and run.
 *
 * This constructor creates and runs the finite element method using the properties
 * provided in meshprops and using the name provided in fun.
 *
 * @param meshprops - Vector containing the information on the square.
 * @param fun - string containing the name of the function to use.
 *
 */
FiniteElemNL::FiniteElemNL(vector<double> meshprops,string fun) {
	// Create mesh.
	MeshMG msh(meshprops);
	msh.uniformrefine();
	// Choose and apply function using polymorphism.
	pde=new FunZeros;
	bdfun=new FunBoltz;
	pdenl=new FunSCNL;
	// Set the mesh variable to the mesh created.
	mesh=msh;
	uword N=mesh.node.n_rows;
	// Initialize solution vector.
	u=zeros<vec>(N);
	// Calculate the right hand side of the equation.
	vec b=calcRHS();
	mat node=mesh.node;
	// Calculate the boundary condition.
	u.rows(mesh.bdNode)=bdfun->evalF(node.rows(mesh.bdNode));
	// Release the function classes created.
	// Setup system to solve.
	//vec r=b-(mesh.stiffness*u);
	double tol=1e-6;
	mat A(mesh.stiffness);
	mat M(mesh.mass);
	vec Mv=M.diag();
	vec fu=pdenl->evalF(u);
	vec residual=((A*u)+(Mv%fu)-b);
	uvec freeNode=mesh.freeNode;
	vec r=residual.rows(freeNode);
	uword k=0;
	tol=tol*norm(r);
	double err=2*tol;
	while(k<10 && err>tol){
		u=GSsolve(b, u, 10, Mv, A, 1e-6, mesh.freeNode);
		u=GSsolveb(b, u, 10, Mv, A, 1e-6, mesh.freeNode);
		residual=((A*u)+(Mv%fu)-b);
		r=residual.rows(freeNode);
		err=norm(r);
		cout << err << endl;
		cout << "iteration " << k << endl;
		k++;
	}
	u.print();
	//u=NWTsolve(b, u, 10, Mv, A, 1e-6, mesh.freeNode);

	delete pde;
	delete bdfun;
	delete pdenl;
}

/**
 * calcRHS() is an internal method to find the right hand side.
 *
 * This is used above to set b for the system Au=b
 */
vec FiniteElemNL::calcRHS(){
	// Get needed Matrices and vectors.
	vec area=mesh.area; mat node=mesh.node; umat elem=mesh.elem;
	// Calculate Midpoints.
	mat mid1=(node.rows(elem.col(1))+node.rows(elem.col(2)))/2.0;
	mat mid2=(node.rows(elem.col(2))+node.rows(elem.col(0)))/2.0;
	mat mid3=(node.rows(elem.col(0))+node.rows(elem.col(1)))/2.0;
	// Calculate vectors to accumulate
	vec bt1=(area%((pde->evalF(mid2))+(pde->evalF(mid3))))/6.0;
	vec bt2=(area%((pde->evalF(mid3))+(pde->evalF(mid1))))/6.0;
	vec bt3=(area%((pde->evalF(mid1))+(pde->evalF(mid2))))/6.0;
	// Vertically concatenate vectors.
	vec bt12=join_vert(bt1,bt2);
	vec bts=join_vert(bt12,bt3);
	uvec elemv=vectorise(elem);
	// Accumulate the array.
	vec ret=accumArray(elemv,bts,node.n_rows);
	return ret;
}

/**
 * accumArray implements the accumarray method from MATLAB.
 *
 * This is a direct implementation and does the exact same thing
 * as the MATLAB function.
 *
 * @param subs - vector of indices.
 * @param ar - vector of values.
 * @param N - size of the array to be made.
 * @return accumulated array (same as MATLAB would).
 */
vec FiniteElemNL::accumArray(uvec subs,vec ar,uword N){

	vec S=zeros<vec>(N);

	for(uword i=0;i<N;i++){
		// Get the subscripts.
		uvec q1=find(subs ==(i));
		vec spot;
		double thesum=0.0;
		if(!q1.is_empty()){
			// Find elements at indices q1
			spot=ar.elem(q1);
			// Sum elements.
			thesum=sum(spot);
			// Set at position i.
			S(i)=thesum;
		}
		// If it is empty, add 0.
		else{
			S(i)=0.0;
		}
	}
	return S;
}


vec FiniteElemNL::NWTsolve(vec b,vec u, uword maxitr,
		vec M, mat A ,double tol,uvec freeNode){
	// Initial setup
	//uword N=u.n_rows;
	vec fu=pdenl->evalF(u);
	//vec Df=pdenl->evalDF(uf);
	vec residual=((A*u)+(M%fu)-b);
	vec r=residual.rows(freeNode);
	uword k=0;
	tol=tol*norm(r);
	double err=2*tol;
	while(k<maxitr && err>tol){
		vec Df=M%(pdenl->evalDF(u));
		mat Dfm=diagmat(Df);
		mat B=A+Dfm;
		vec e=solve(B.submat(freeNode, freeNode), r);
		u.rows(freeNode)=u.rows(freeNode)-e;
		fu=pdenl->evalF(u);
		residual=((A*u)+(M%fu)-b);
		r=residual.rows(freeNode);
		err=norm(r);
		k=k+1;
		cout << "Iteration: " << k << "error: " << err << endl;
	}
	return u;

}

vec FiniteElemNL::GSsolve(vec b,vec u, uword maxitr,
		vec M, mat A ,double tol,uvec freeNode){
	uword Nu=u.n_rows;
	for(uword i=1;i<Nu-1;i++){
		uvec isfree=find(freeNode==i);
		if(!isfree.empty()){
			mat Ar=A.row(i);
			mat currA=(Ar.cols(span(0,i-1))*u.rows(span(0,i-1)))+
						(Ar.cols(span(i+1,Nu-1))*u.rows(span(i+1,Nu-1)));
			//double ci=currA(0,0)+b(i);
			double ci=currA(0,0)+b(i);
			vector<double> pars(3);
			pars[0]=ci;
			pars[1]=A(i,i);
			pars[2]=M(i);
			FunSCNL1D nfun(pars);
			u(i)=NWTsolve1D(nfun,u(i),1e-6,10);
		}
	}
	return u;
}

vec FiniteElemNL::GSsolveb(vec b,vec u, uword maxitr,
		vec M, mat A ,double tol,uvec freeNode){
	uword Nu=u.n_rows;
	for(uword i=Nu-2;i>=0;i--){
		uvec isfree=find(freeNode==i);
		if(!isfree.empty()){
			mat Ar=A.row(i);
			mat currA=(Ar.cols(span(0,i-1))*u.rows(span(0,i-1)))
						+(Ar.cols(span(i,Nu-1))*u.rows(span(i,Nu-1)));
			double ci=currA(0,0)+b(i);
			vector<double> pars(3);
			pars[0]=ci;
			pars[1]=A(i,i);
			pars[2]=M(i);
			FunSCNL1D nfun(pars);
			u(i)=NWTsolve1D(nfun,u(i),1e-6,10);
		}
	}
	return u;
}

double FiniteElemNL::NWTsolve1D(FunSCNL1D f, double x0, double tol, uword maxitr){
	double fp=f.evalF(x0);
	double df=f.evalDF(x0);
	uword n=0;
	double err=abs(fp);
	cout << "Newton time" << endl;
	double x=x0;
	while(err>tol && n<maxitr){
		x=x-(fp/df);
		df=f.evalDF(x);
		fp=f.evalF(x);
		err=abs(fp);
		n++;
	}
	return x;
}

FiniteElemNL::~FiniteElemNL() {
}

