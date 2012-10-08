//*****************************************************
//
// bibutil.cpp
// Biblioteca de rotinas auxiliares que inclui funções 
// para realizar as seguintes tarefas:
// - Normalizar um vetor;
// - Calcular o produto vetorial entre dois vetores;
// - Calcular um vetor normal a partir de três vértices;
// - Rotacionar um vértice ao redor de um eixo (x, y ou z);
// - Ler um modelo de objeto 3D de um arquivo no formato 
//		OBJ e armazenar em uma estrutura;
// - Desenhar um objeto 3D recebido por parâmetro;
// - Liberar a memória ocupada por um objeto 3D;
// - Calcular o vetor normal de cada face de um objeto 3D;
// - Decodificar e armazenar numa estrutura uma imagem JPG 
//		para usar como textura;
// - Armazenar em uma estrutura uma imagem JPG para usar 
//		como textura.
//
// Marcelo Cohen e Isabel H. Manssour
// Este código acompanha o livro
// "OpenGL - Uma Abordagem Prática e Objetiva" 
// 
//*****************************************************

#include <math.h>
#include <string.h>
#include <vector>
#include "bibutil.h"

#define DEBUG

using namespace std;

// Lista de objetos
vector<OBJ*> _objetos(0);

// Lista de materiais
vector<MAT*> _materiais(0);

// Lista de texturas
vector<TEX*> _texturas(0);

// Modo de desenho
char _modo = 't';

// Variáveis para controlar a taxa de quadros por segundo
int _numquadro =0, _tempo, _tempoAnterior = 0;
float _ultqps = 0;

#ifndef __FREEGLUT_EXT_H__
// Função para desenhar um texto na tela com fonte bitmap
void glutBitmapString(void *fonte,char *texto)
{
	// Percorre todos os caracteres
	while (*texto)
    	glutBitmapCharacter(fonte, *texto++);
}
#endif

// Calcula e retorna a taxa de quadros por segundo
float CalculaQPS(void)
{
	// Incrementa o contador de quadros
	_numquadro++;

	// Obtém o tempo atual
	_tempo = glutGet(GLUT_ELAPSED_TIME);
	// Verifica se passou mais um segundo
	if (_tempo - _tempoAnterior > 1000)
	{
		// Calcula a taxa atual
		_ultqps = _numquadro*1000.0/(_tempo - _tempoAnterior);
		// Ajusta as variáveis de tempo e quadro
	 	_tempoAnterior = _tempo;
		_numquadro = 0;
	}
	// Retorna a taxa atual
	return _ultqps;
}

// Escreve uma string na tela, usando uma projeção ortográfica
// normalizada (0..1, 0..1)
void Escreve2D(float x, float y, char *str)
{
	glMatrixMode(GL_PROJECTION);
	// Salva projeção perspectiva corrente
	glPushMatrix();
	glLoadIdentity();
	// E cria uma projeção ortográfica normalizada (0..1,0..1)
	gluOrtho2D(0,1,0,1);
	
	glMatrixMode(GL_MODELVIEW);
	// Salva modelview corrente
	glPushMatrix();
	glLoadIdentity();

	// Posiciona no canto inferior esquerdo da janela
	glRasterPos2f(x,y);
	glColor3f(0,0,0);
	// "Escreve" a mensagem
	glutBitmapString(GLUT_BITMAP_9_BY_15,str);
	
	glMatrixMode(GL_PROJECTION);
	// Restaura a matriz de projeção anterior
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	// ... e a modelview anterior
	glPopMatrix();
}

// Normaliza o vetor recebido por parâmetro.
void Normaliza(VERT &norm)
{
	float tam = sqrt(norm.x*norm.x
			+norm.y*norm.y
			+norm.z*norm.z);
	if(tam == 0) return;
	norm.x /= tam;
	norm.y /= tam;
	norm.z /= tam;
}

// Rotina que recebe dois vetores e retorna por parâmetro, 
// através de vresult, o produto vetorial entre eles.
void ProdutoVetorial (VERT &v1, VERT &v2, VERT &vresult)
{
	vresult.x = v1.y * v2.z - v1.z * v2.y;
	vresult.y = v1.z * v2.x - v1.x * v2.z;
	vresult.z = v1.x * v2.y - v1.y * v2.x;
}

// Esta rotina recebe três vértices coplanares e calcula
// o vetor normal a eles (n).
void VetorNormal(VERT vert1, VERT vert2, VERT vert3, VERT &n)
{
    VERT vet1, vet2;
    
    vet1.x = vert1.x - vert2.x;
    vet1.y = vert1.y - vert2.y;
    vet1.z = vert1.z - vert2.z;
    vet2.x = vert3.x - vert2.x;
    vet2.y = vert3.y - vert2.y;
    vet2.z = vert3.z - vert2.z;
    ProdutoVetorial(vet2, vet1, n);
    Normaliza(n);
}

// Rotaciona um vértice em torno de Z por <ang> graus
void RotaZ(VERT &in, VERT &out, float ang)
{
	float arad = ang*M_PI/180.0;
	out.x =  in.x * cos(arad) + in.y * sin(arad);
	out.y = -in.x * sin(arad) + in.y * cos(arad);
	out.z =  in.z;
}

// Rotaciona um vértice em torno de Y por <ang> graus
void RotaY(VERT &in, VERT &out, float ang)
{
	float arad = ang*M_PI/180.0;
	out.x = in.x * cos(arad) - in.z * sin(arad);
	out.y = in.y;
	out.z = in.x * sin(arad) + in.z * cos(arad);
}

// Rotaciona um vértice em torno de X por <ang> graus
void RotaX(VERT &in, VERT &out, float ang)
{
	float arad = ang*M_PI/180.0;
	out.x =  in.x;
	out.y =  in.y * cos(arad) + in.z * sin(arad);
	out.z = -in.y * sin(arad) + in.z * cos(arad);
}

// Função interna, usada por CarregaObjeto para
// interpretar a definição de uma face em um
// arquivo .OBJ
// 
// Recebe um apontador para a posição corrente na
// string e devolve o valor numérico encontrado
// até o primeiro / ou -1 caso não exista
//
// Também retorna em sep o separador encontrado
int leNum(char **face, char *sep)
{
	char temp[10];
	int pos = 0;
	while(**face)
	{
		if(**face == '/' || **face==' ') // achamos o separador
		{
			*sep = **face;
			(*face)++;
			break;
		}
		temp[pos++] = **face;
		(*face)++;
	}
	temp[pos]=0;
	if(!pos) // string vazia ?
		return -1;
	return atoi(temp);
}

// Procura um material pelo nome na lista e devolve
// o índice onde está, ou -1 se não achar
int _procuraMaterial(char *nome)
{
	unsigned int i;
	for(i=0;i<_materiais.size();++i)
		if(!strcmp(nome,_materiais[i]->nome))
			return i;
	return -1;
}

// Procura um material pelo nome e devolve um
// apontador para ele ou NULL caso não ache
MAT *ProcuraMaterial(char *nome)
{
	int pos = _procuraMaterial(nome);
	if(pos == -1) return NULL;
	else return _materiais[pos];
}

// Procura uma textura pelo nome na lista e devolve
// o índice onde está, ou -1 se não achar
int _procuraTextura(char *nome)
{
	unsigned int i;
	for(i=0;i<_texturas.size();++i)
		if(!strcmp(nome,_texturas[i]->nome))
			return i;
	return -1;
}

// Lê um arquivo que define materiais para um objeto 3D no
// formato .OBJ
void _leMateriais(char *nomeArquivo)
{
	char aux[256];
	FILE *fp;
	MAT *ptr;
	fp = fopen(nomeArquivo,"r");

	/* Especificação do arquivo de materiais (.mtl):
	 * 
	 * #,!,$  - comentário
	 * newmtl - nome do novo material(sem espaços)
	 * Ka	  - Cor ambiente, especificada como 3 floats
	 * Kd	  - Cor difusa, idem
	 * Ks	  - Cor especular, idem
	 * Ns	  - Coeficiente de especularidade, entre 0 and 1000 
	 *          (convertido para a faixa válida em OpenGL (0..128) durante a leitura)
	 * d	  - Transparência - mesmo significado que o alpha em OpenGL: 0.0 a 1.0 (0.0 = transparente)
	 *
	 * Os demais campos são ignorados por este leitor:
	 * Tr	  - Transparência (0 = opaco, 1 = transparente)
	 * map_Kd - Especifica arquivo com a textura a ser mapeada como material difuso
	 * refl	  - Especifica arquivo com um mapa de reflexão
	 * Ni	  - Índice de refração, 1.0 ou maior, onde 1.0 representa sem refração
	 * bump	  - Especifica arquivo para "bump mapping"
	 * illum  - Modelo de iluminação (1 = apenas cores, 2 = textura, 3 = reflexões e raytracing)
	 * map_Ka - Especifica arquivo com a textura a ser mapeada como cor ambiente
	 * map_Ks - Especifica arquivo com a textura a ser mapeada como cor especular
	 * map_Ns - Especifica arquivo com a textura a ser mapeada como reflexão especular
	 * map_d  - Especifica arquivo cmo a textura a ser mapeada como transparência (branco é opaco, preto é transparente)
	 *
	 */

	while(!feof(fp))
	{
		fgets(aux,255,fp);
		if (strlen(aux)>1)
			aux[strlen(aux)-1]=0;	// elimina o \n lido do arquivo
		if(aux[0]=='#') continue;
		if(!strncmp(aux,"newmtl",6)) // Novo material ?
		{
			// Se material já existe na lista, pula para o 
			// próximo
			if(_procuraMaterial(&aux[7])!=-1)
			{
				ptr = NULL;
				continue;
			}
			if((ptr = (MAT *) malloc(sizeof(MAT)))==NULL)
			{
				printf("Sem memória para novo material!");
				exit(1);
			}
			// Adiciona à lista
			_materiais.push_back(ptr);
			// Copia nome do material
			strcpy(ptr->nome,&aux[7]);
			// Não existe "emission" na definição do material
			// mas o valor pode ser setado mais tarde,
			// via SetaEmissaoMaterial(..)
			ptr->ke[0] = ptr->ke[1] = ptr->ke[2] = 0.0;
		}
		if(!strncmp(aux,"Ka ",3)) // Ambiente
		{
			if(ptr==NULL) continue;
			sscanf(aux,"Ka %f %f %f",&ptr->ka[0],&ptr->ka[1],&ptr->ka[2]);
		}
		if(!strncmp(aux,"Kd ",3)) // Difuso
		{
			if(ptr==NULL) continue;
			sscanf(aux,"Kd %f %f %f",&ptr->kd[0],&ptr->kd[1],&ptr->kd[2]);
		}
		if(!strncmp(aux,"Ks ",3)) // Especular
		{
			if(ptr==NULL) continue;
			sscanf(aux,"Ks %f %f %f",&ptr->ks[0],&ptr->ks[1],&ptr->ks[2]);
		}
		if(!strncmp(aux,"Ns ",3)) // Fator de especularidade
		{
			if(ptr==NULL) continue;
			sscanf(aux,"Ns %f",&ptr->spec);
			// Converte da faixa lida (0...1000) para o intervalo
			// válido em OpenGL (0...128)
			ptr->spec = ptr->spec/1000.0 * 128;
		}
		if(!strncmp(aux,"d ",2)) // Alpha
		{
			if(ptr==NULL) continue;
			// Não existe alpha na definição de cada componente
			// mas o valor é lido em separado e vale para todos
			float alpha;
			sscanf(aux,"d %f",&alpha);
			ptr->ka[3] = alpha;
			ptr->kd[3] = alpha;
			ptr->ks[3] = alpha;
		}
	}
	fclose(fp);
}

// Cria e carrega um objeto 3D que esteja armazenado em um
// arquivo no formato OBJ, cujo nome é passado por parâmetro.
// É feita a leitura do arquivo para preencher as estruturas 
// de vértices e faces, que são retornadas através de um OBJ.
//
// O parâmetro mipmap indica se deve-se gerar mipmaps a partir
// das texturas (se houver)
OBJ *CarregaObjeto(char *nomeArquivo, bool mipmap)
{
	int i;
	int vcont,ncont,fcont,tcont;
	int material, texid;
	char aux[256];
	TEX *ptr;
	FILE *fp;
	OBJ *obj;

	fp = fopen(nomeArquivo, "r");  // abre arquivo texto para leitura

#ifdef DEBUG
	printf("*** Objeto: %s\n",nomeArquivo);
#endif
	/* Especificação do formato .obj para objetos poligonais:
	 * 
	 * # - commentário
	 * v x y z - vértice
	 * vn x y z - normal
	 * vt x y z - texcoord
	 * f a1 a2 ... - face com apenas índices de vértices
	 * f a1/b1/c1 a2/b2/c2 ... - face com índices de vértices, normais e texcoords
	 * f a1//c1 a2//c2 ... - face com índices de vértices e normais (sem texcoords)
	 *
	 * Campos ignorados:
	 * g nomegrupo1 nomegrupo2... - especifica que o objeto é parte de um ou mais grupos
	 * s numerogrupo ou off - especifica um grupo de suavização ("smoothing")
	 * o nomeobjeto: define um nome para o objeto
	 *
	 * Biblioteca de materiais e textura:
	 * mtllib - especifica o arquivo contendo a biblioteca de materiais
	 * usemtl - seleciona um material da biblioteca
	 * usemat - especifica o arquivo contendo a textura a ser mapeada nas próximas faces
	 * 
	 */

	if(fp == NULL)
          return NULL;

	if ( ( obj = (OBJ *) malloc(sizeof(OBJ)) ) == NULL)
		return NULL;

	// Inicializa contadores do objeto
	obj->numVertices  = 0;
	obj->numFaces     = 0;
	obj->numNormais   = 0;
	obj->numTexcoords = 0;
	// A princípio não temos normais por vértice...
	obj->normais_por_vertice = false;
	// E também não temos materiais...
	obj->tem_materiais = false;
	obj->textura = -1;	// sem textura associada
	obj->dlist = -1;	// sem display list

	obj->vertices = NULL;
	obj->faces = NULL;
	obj->normais = NULL;
	obj->texcoords = NULL;

	// A primeira passagem serve apenas para contar quantos
	// elementos existem no arquivo - necessário para
	// alocar memória depois
	while(!feof(fp))
	{
		fgets(aux,255,fp);
		if(!strncmp(aux,"v ",2)) // encontramos um vértice
			obj->numVertices++;
		if(!strncmp(aux,"f ",2)) // encontramos uma face
			obj->numFaces++;
		if(!strncmp(aux,"vn ",3)) // encontramos uma normal
			obj->numNormais++;
		if(!strncmp(aux,"vt ",3)) // encontramos uma texcoord
			obj->numTexcoords++;
	}
	// Agora voltamos ao início do arquivo para ler os elementos
	rewind(fp);

#ifdef DEBUG
	printf("Vertices: %d\n",obj->numVertices);
	printf("Faces:    %d\n",obj->numFaces);
	printf("Normais:  %d\n",obj->numNormais);
	printf("Texcoords:%d\n",obj->numTexcoords);
#endif

	// Aloca os vértices
	if ( ( obj->vertices = (VERT *) malloc((sizeof(VERT)) * obj->numVertices) ) == NULL )
		return NULL;

	// Aloca as faces
	if ( ( obj->faces = (FACE *) malloc((sizeof(FACE)) * obj->numFaces) ) == NULL )
		return NULL;

	// Aloca as normais
	if(obj->numNormais)
		if ( ( obj->normais = (VERT *) malloc((sizeof(VERT)) * obj->numNormais) ) == NULL )
			return NULL;

	// Aloca as texcoords
	if(obj->numTexcoords)
		if ( ( obj->texcoords = (TEXCOORD *) malloc((sizeof(TEXCOORD)) * obj->numTexcoords) ) == NULL )
			return NULL;

	// A segunda passagem é para ler efetivamente os
	// elementos do arquivo, já que sabemos quantos
	// tem de cada tipo
	vcont = 0;
	ncont = 0;
	tcont = 0;
	fcont = 0;
	// Material corrente = nenhum
	material = -1;
	// Textura corrente = nenhuma
	texid = -1;

	// Utilizadas para determinar os limites do objeto
	// em x,y e z
	float minx,miny,minz;
	float maxx,maxy,maxz;

	while(!feof(fp))
	{
		fgets(aux,255,fp);
		aux[strlen(aux)-1]=0;	// elimina o \n lido do arquivo
		// Pula comentários
		if(aux[0]=='#') continue;

		// Definição da biblioteca de materiais ?
		if(!strncmp(aux,"mtllib",6))
		{
				// Chama função para ler e interpretar o arquivo
				// que define os materiais
				_leMateriais(&aux[7]);
				// Indica que o objeto possui materiais
				obj->tem_materiais = true;
		}
		// Seleção de material ?
		if(!strncmp(aux,"usemtl",6))
		{
			// Procura pelo nome e salva o índice para associar
			// às próximas faces
			material = _procuraMaterial(&aux[7]);
			texid = -1;
		}
		// Seleção de uma textura (.jpg)
		if(!strncmp(aux,"usemat",6))
		{
			// Às vezes, é especificado o valor (null) como textura
			// (especialmente em se tratando do módulo de exportação
			// do Blender)
			if(!strcmp(&aux[7],"(null)"))
			{
				texid = -1;
				continue;
			}
			// Tenta carregar a textura
			ptr = CarregaTextura(&aux[7],mipmap);
			texid = ptr->texid;
		}
		// Vértice ?
		if(!strncmp(aux,"v ",2))
		{
			sscanf(aux,"v %f %f %f",&obj->vertices[vcont].x,
					&obj->vertices[vcont].y,
					&obj->vertices[vcont].z);
			if(!vcont)
			{
				minx = maxx = obj->vertices[vcont].x;
				miny = maxy = obj->vertices[vcont].y;
				minz = maxz = obj->vertices[vcont].z;
			}
			else
			{
				if(obj->vertices[vcont].x < minx) minx = obj->vertices[vcont].x;
				if(obj->vertices[vcont].y < miny) miny = obj->vertices[vcont].y;
				if(obj->vertices[vcont].z < minz) minz = obj->vertices[vcont].z;
				if(obj->vertices[vcont].x > maxx) maxx = obj->vertices[vcont].x;
				if(obj->vertices[vcont].y > maxy) maxy = obj->vertices[vcont].y;
				if(obj->vertices[vcont].z > maxz) maxz = obj->vertices[vcont].z;
			}
			vcont++;
		}
		// Normal ?
		if(!strncmp(aux,"vn ",3))
		{
			sscanf(aux,"vn %f %f %f",&obj->normais[ncont].x,
					&obj->normais[ncont].y,
					&obj->normais[ncont].z);
			ncont++;
			// Registra que o arquivo possui definição de normais por
			// vértice
			obj->normais_por_vertice = true;
		}
		// Texcoord ?
		if(!strncmp(aux,"vt ",3))
		{
			sscanf(aux,"vt %f %f %f",&obj->texcoords[tcont].s,
					&obj->texcoords[tcont].t,
					&obj->texcoords[tcont].r);
			tcont++;
		}
		// Face ?
		if(!strncmp(aux,"f ",2))
		{
			// Pula "f "
			char *ptr = aux+2;
			// Associa à face o índice do material corrente, ou -1 se
			// não houver
			obj->faces[fcont].mat = material;
			// Associa à face o texid da textura seleciona ou -1 se
			// não ouver
			obj->faces[fcont].texid = texid;
			// Temporários para armazenar os índices desta face
			int vi[10],ti[10],ni[10];
			// Separador encontrado
			char sep;
			obj->faces[fcont].nv = 0;
			int nv = 0;
			bool tem_t = false;
			bool tem_n = false;
			// Interpreta a descrição da face
			while(*ptr)
			{
				// Lê índice do vértice
				vi[nv] = leNum(&ptr,&sep);
				// Se o separador for espaço, significa que
				// não temos texcoord nem normais
				if(sep==' ')
				{
					// Nesse caso, incrementamos o contador
					// de vértices e continuamos
					nv++;
					continue;
				}
				// Lê índice da texcoord
				int aux = leNum(&ptr,&sep);
				// Se o valor for != -1, significa
				// que existe um índice de texcoord
				if(aux!=-1)
				{
					ti[nv] = aux;
					tem_t = true;
				}
				// Mas se o separador for " ", significa
				// que não existe um índice de normal
				if(sep==' ')
				{
					// E nesse caso, pulamos para
					// o próximo vértice
					nv++;
					continue;
				}
				// Lê o índice da normal
				aux = leNum(&ptr,&sep);
				if(aux!=-1)
				{
					ni[nv] = aux;
					tem_n = true;
				}
				// Prepara para próximo vértice
				nv++;
			}
			// Fim da face, aloca memória para estruturas e preenche com
			// os valores lidos
			obj->faces[fcont].nv = nv;
			obj->faces[fcont].vert = (int *) malloc(sizeof(int)*nv);
			// Só aloca memória para normais e texcoords se for necessário
			if(tem_n) obj->faces[fcont].norm = (int *) malloc(sizeof(int)*nv);
				else obj->faces[fcont].norm = NULL;
			if(tem_t) obj->faces[fcont].tex  = (int *) malloc(sizeof(int)*nv);
				else obj->faces[fcont].tex = NULL;
			// Copia os índices dos arrays temporários para a face
			for(i=0;i<nv;++i)
			{
				// Subtraímos -1 dos índices porque o formato OBJ começa
				// a contar a partir de 1, não 0
				obj->faces[fcont].vert[i] = vi[i]-1;
				if(tem_n) obj->faces[fcont].norm[i] = ni[i]-1;
				if(tem_t) obj->faces[fcont].tex[i]  = ti[i]-1;
			}
			// Prepara para próxima face
			fcont++;
		}
	}
#ifdef DEBUG
	printf("Limites: %f %f %f - %f %f %f\n",minx,miny,minz,maxx,maxy,maxz);
#endif
	// Fim, fecha arquivo e retorna apontador para objeto
	fclose(fp);
	// Adiciona na lista
	_objetos.push_back(obj);
	return obj;
}

// Seta o modo de desenho a ser utilizado para os objetos
// 'w' - wireframe
// 's' - sólido
// 't' - sólido + textura
void SetaModoDesenho(char modo)
{
	if(modo!='w' && modo!='s' && modo!='t') return;
	_modo = modo;
}

// Desenha um objeto 3D passado como parâmetro.
void DesenhaObjeto(OBJ *obj)
{
	int i;	// contador
	GLint ult_texid, texid;	// última/atual textura 
	GLenum prim = GL_POLYGON;	// tipo de primitiva
	GLfloat branco[4] = { 1.0, 1.0, 1.0, 1.0 };	// constante para cor branca

	// Gera nova display list se for o caso
	if(obj->dlist >= 1000)
		glNewList(obj->dlist-1000,GL_COMPILE_AND_EXECUTE);
	// Ou chama a display list já associada...
	else if(obj->dlist > -1)
	{
		glCallList(obj->dlist);
		return;
	}

	// Seleciona GL_LINE_LOOP se o objetivo
	// for desenhar o objeto em wireframe
	if(_modo=='w') prim = GL_LINE_LOOP;

	// Salva atributos de iluminação e materiais
	glPushAttrib(GL_LIGHTING_BIT);
	glDisable(GL_TEXTURE_2D);
	// Se objeto possui materiais associados a ele,
	// desabilita COLOR_MATERIAL - caso contrário,
	// mantém estado atual, pois usuário pode estar
	// utilizando o recurso para colorizar o objeto
	if(obj->tem_materiais)
		glDisable(GL_COLOR_MATERIAL);

	// Armazena id da última textura utilizada
	// (por enquanto, nenhuma)
	ult_texid = -1;
	// Varre todas as faces do objeto
	for(i=0; i<obj->numFaces; i++)
	{
		// Usa normais calculadas por face (flat shading) se
		// o objeto não possui normais por vértice
		if(!obj->normais_por_vertice)
			glNormal3f(obj->normais[i].x,obj->normais[i].y,obj->normais[i].z);

		// Existe um material associado à face ?
		if(obj->faces[i].mat != -1)
		{
			// Sim, envia parâmetros para OpenGL
			int mat = obj->faces[i].mat;
			glMaterialfv(GL_FRONT,GL_AMBIENT,_materiais[mat]->ka);
			// Se a face tem textura, ignora a cor difusa do material
			// (caso contrário, a textura é colorizada em GL_MODULATE)
			if(obj->faces[i].texid != -1 && _modo=='t')
				glMaterialfv(GL_FRONT,GL_DIFFUSE,branco);
			else
				glMaterialfv(GL_FRONT,GL_DIFFUSE,_materiais[mat]->kd);
			glMaterialfv(GL_FRONT,GL_SPECULAR,_materiais[mat]->ks);
			glMaterialfv(GL_FRONT,GL_EMISSION,_materiais[mat]->ke);
			glMaterialf(GL_FRONT,GL_SHININESS,_materiais[mat]->spec);
		}

		// Se o objeto possui uma textura associada, utiliza
		// o seu texid ao invés da informação em cada face
		if(obj->textura != -1)
			texid = obj->textura;
		else
			// Lê o texid associado à face (-1 se não houver)
			texid = obj->faces[i].texid;

		// Se a última face usou textura e esta não,
		// desabilita
		if(texid == -1 && ult_texid != -1)
			glDisable(GL_TEXTURE_2D);

		// Ativa texturas 2D se houver necessidade
		if (texid != -1 && texid != ult_texid && _modo=='t')
		{
		       glEnable(GL_TEXTURE_2D);
		       glBindTexture(GL_TEXTURE_2D,texid);
		}

		// Inicia a face
		glBegin(prim);
		// Para todos os vértices da face
		for(int vf=0; vf<obj->faces[i].nv;++vf)
		{
			// Se houver normais definidas para cada vértice,
			// envia a normal correspondente
			if(obj->normais_por_vertice)
				glNormal3f(obj->normais[obj->faces[i].norm[vf]].x,
				obj->normais[obj->faces[i].norm[vf]].y,
				obj->normais[obj->faces[i].norm[vf]].z);

			// Se houver uma textura associada...
			if(texid!=-1)
				// Envia as coordenadas associadas ao vértice
				glTexCoord2f(obj->texcoords[obj->faces[i].tex[vf]].s,
				obj->texcoords[obj->faces[i].tex[vf]].t);
			// Envia o vértice em si
			glVertex3f(obj->vertices[obj->faces[i].vert[vf]].x,
		    	obj->vertices[obj->faces[i].vert[vf]].y,
			obj->vertices[obj->faces[i].vert[vf]].z);
		}
		// Finaliza a face
		glEnd();

		// Salva a última texid utilizada
		ult_texid = texid;
	} // fim da varredura de faces
	
	// Finalmente, desabilita as texturas
	glDisable(GL_TEXTURE_2D);
	// Restaura os atributos de iluminação e materiais
	glPopAttrib();
	// Se for uma nova display list...
	if(obj->dlist >= 1000)
	{
		// Finaliza a display list
		glEndList();
		// E armazena a identificação correta
		obj->dlist-=1000;
	}
}

// Função interna para liberar a memória ocupada
// por um objeto
void _liberaObjeto(OBJ *obj)
{
	// Libera arrays
	if (obj->vertices != NULL)  free(obj->vertices);
	if (obj->normais != NULL)   free(obj->normais);
	if (obj->texcoords != NULL) free(obj->texcoords);
	// Para cada face...
	for(int i=0; i<obj->numFaces;++i)
	{
		// Libera as listas de vértices da face
		if (obj->faces[i].vert != NULL) free(obj->faces[i].vert);
		// Libera as listas de normais da face
		if (obj->faces[i].norm != NULL) free(obj->faces[i].norm);
		// Libera as listas de texcoords da face
		if (obj->faces[i].tex  != NULL) free(obj->faces[i].tex);
	}
	// Libera array de faces
	if (obj->faces != NULL) free(obj->faces);
	// Finalmente, libera a estrutura principal
	free(obj);
}

// Libera memória ocupada por um objeto 3D
void LiberaObjeto(OBJ *obj)
{
	unsigned int o;
	if(obj==NULL)	// se for NULL, libera todos os objetos
	{
		for(o=0;o<_objetos.size();++o)
			_liberaObjeto(_objetos[o]);
		_objetos.clear();
	}
	else
	{
		// Procura pelo objeto no vector
		vector<OBJ*>::iterator it = _objetos.begin();
		for(it = _objetos.begin(); it<_objetos.end(); ++it)
			// e sai do loop quando encontrar
			if(*it == obj)
				break;
		// Apaga do vector
		_objetos.erase(it);
		// E libera as estruturas internas
		_liberaObjeto(obj);
	}
}

// Libera memória ocupada pela lista de materiais e texturas
void LiberaMateriais()
{
	unsigned int i;
#ifdef DEBUG
	printf("Total de materiais: %d\n",_materiais.size());
#endif
	// Para cada material
	for(i=0;i<_materiais.size();++i)
	{
#ifdef DEBUG
		printf("%s a: (%f,%f,%f,%f) - d: (%f,%f,%f,%f) - e: (%f,%f,%f,%f - %f)\n",_materiais[i]->nome,
				_materiais[i]->ka[0],_materiais[i]->ka[1],_materiais[i]->ka[2], _materiais[i]->ka[3],
				_materiais[i]->kd[0],_materiais[i]->kd[1],_materiais[i]->kd[2], _materiais[i]->kd[3],
				_materiais[i]->ks[0],_materiais[i]->ks[1],_materiais[i]->ks[2], _materiais[i]->ks[3],
				_materiais[i]->spec);
#endif
		// Libera material
		free(_materiais[i]);
	}
	// Limpa lista
	_materiais.clear();
#ifdef DEBUG
	printf("Total de texturas: %d\n",_texturas.size());
#endif
	// Para cada textura
	for(i=0;i<_texturas.size();++i)
	{
		// Libera textura - não é necessário liberar a imagem, pois esta já
		// foi liberada durante a carga da textura - ver CarregaTextura
#ifdef DEBUG
		printf("%s: %d x %d (id: %d)\n",_texturas[i]->nome,_texturas[i]->dimx,
				_texturas[i]->dimy,_texturas[i]->texid);
#endif
		free(_texturas[i]);
	}
	// Limpa lista
	_texturas.clear();
}

// Calcula o vetor normal de cada face de um objeto 3D.
void CalculaNormaisPorFace(OBJ *obj)
{
	int i;
	// Retorna se o objeto já possui normais por vértice
	if(obj->normais_por_vertice) return;
	// Aloca memória para as normais (uma por face)
	if ( ( obj->normais = (VERT *) malloc((sizeof(VERT)) * obj->numFaces) ) == NULL )
			return;
	// Varre as faces e calcula a normal, usando os 3 primeiros vértices de
	// cada uma
	for(i=0; i<obj->numFaces; i++)
	VetorNormal(obj->vertices[obj->faces[i].vert[0]],
		obj->vertices[obj->faces[i].vert[1]],
		obj->vertices[obj->faces[i].vert[2]],obj->normais[i]);
}

// Função para ler um arquivo JPEG e criar uma
// textura OpenGL
// mipmap = true se deseja-se utilizar mipmaps
TEX *CarregaTextura(char *arquivo, bool mipmap)
{
	GLenum formato;

	if(!arquivo)		// retornamos NULL caso nenhum nome de arquivo seja informado
		return NULL;

	int indice = _procuraTextura(arquivo);
	// Se textura já foi carregada, retorna
	// apontador para ela
	if(indice!=-1)
		return _texturas[indice];

	TEX *pImage = CarregaJPG(arquivo);	// carrega o arquivo JPEG

	if(pImage == NULL)	// se não foi possível carregar o arquivo, finaliza o programa
		exit(0);

	strcpy(pImage->nome,arquivo);
	// Gera uma identificação para a nova textura
	glGenTextures(1, &pImage->texid);

	// Informa o alinhamento da textura na memória
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	// Informa que a textura é a corrente
	glBindTexture(GL_TEXTURE_2D, pImage->texid);

	if(pImage->ncomp==1) formato = GL_LUMINANCE;
	else formato = GL_RGB;

	if(mipmap)
	{
		// Cria mipmaps para obter maior qualidade
		gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, pImage->dimx, pImage->dimy,
			formato, GL_UNSIGNED_BYTE, pImage->data);
		// Ajusta os filtros iniciais para a textura
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	}
	else
	{
		// Envia a textura para OpenGL, usando o formato RGB
		glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, pImage->dimx, pImage->dimx,
			0, formato, GL_UNSIGNED_BYTE, pImage->data);
		// Ajusta os filtros iniciais para a textura
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	// Finalmente, libera a memória ocupada pela imagem (já que a textura já foi enviada para OpenGL)

	free(pImage->data); 	// libera a memória ocupada pela imagem

	// Inclui textura na lista
	_texturas.push_back(pImage);
	// E retorna apontador para a nova textura
	return pImage;
}

// Identificadores OpenGL para cada uma das faces
// do cubemap
GLenum faces[6] = {
  GL_TEXTURE_CUBE_MAP_POSITIVE_X,
  GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
  GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
  GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
  GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
  GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
};

char *nomes[] = {
		"posx", "negx", "posy", "negy", "posz", "negz" };

// Função para ler 6 arquivos JPEG e criar
// texturas para cube mapping
// mipmap = true se for utilizar mipmaps
TEX *CarregaTexturasCubo(char *nomebase, bool mipmap)
{
	GLenum formato;
	TEX *primeira;

	if(!nomebase)		// retornamos NULL caso nenhum nome de arquivo seja informado
		return NULL;

	// Informa o alinhamento da textura na memória
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// Carrega as texturas do cubemap e envia para OpenGL
	for(int i=0;i<6;++i)
	{
		char arquivo[100];
		sprintf(arquivo,"%s_%s.jpg",nomebase,nomes[i]);

		int indice = _procuraTextura(nomebase);
		// Se textura já foi carregada, retorna
		if(indice!=-1)
			return _texturas[indice];

		// Carrega o arquivo JPEG, sem inverter a
		// ordem das linhas (necessário para a
		// textura não ficar de cabeça para baixo
		// no cube map)
		TEX *pImage = CarregaJPG(arquivo,false);

		if(pImage == NULL)	// se não foi possível carregar o arquivo, finaliza o programa
			exit(0);

		// Se for a primeira textura...
		if(!i)
		{
			// Salva apontador para retornar depois
			primeira = pImage;
			// Gera uma identificação para a nova textura
			glGenTextures(1, &pImage->texid);
			glBindTexture(GL_TEXTURE_CUBE_MAP, pImage->texid);
			strcpy(pImage->nome,arquivo);
		}

		if(pImage->ncomp==1) formato = GL_LUMINANCE;
		else formato = GL_RGB;

		if(mipmap)
			// Cria mipmaps para obter maior qualidade
			gluBuild2DMipmaps(faces[i], GL_RGB, pImage->dimx, pImage->dimy,
			formato, GL_UNSIGNED_BYTE, pImage->data);
		else
			// Envia a textura para OpenGL, usando o formato RGB
			glTexImage2D (faces[i], 0, GL_RGB, pImage->dimx, pImage->dimx,
				0, formato, GL_UNSIGNED_BYTE, pImage->data);

		// Finalmente, libera a memória ocupada pela imagem (já que a textura já foi enviada para OpenGL)

		free(pImage->data); 	// libera a memória ocupada pela imagem

		// Inclui somente a primeira textura na lista
		if(!i) _texturas.push_back(pImage);
		else free(pImage);
	}

	// Ajusta os filtros iniciais para o cube map
	if(mipmap)
	{
		glTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	}
	else
	{
		glTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri (GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	// Retorna apontador para a primeira imagem (que contém a id)
	return primeira;
}

// Seta o filtro de uma textura específica
// ou de todas na lista (se for passado o argumento -1)
void SetaFiltroTextura(GLint tex, GLint filtromin, GLint filtromag)
{
	glEnable(GL_TEXTURE_2D);
	if(tex!=-1)
	{
		glBindTexture(GL_TEXTURE_2D,tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtromin);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtromag);
	}
	else
	for(unsigned int i=0;i<_texturas.size();++i)
	{
		glBindTexture(GL_TEXTURE_2D,_texturas[i]->texid);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtromin);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtromag);
	}
	glDisable(GL_TEXTURE_2D);
}

// Desabilita a geração de uma display list
// para o objeto especificado
void DesabilitaDisplayList(OBJ *ptr)
{
	if(ptr == NULL) return;
	if(ptr->dlist > 0 && ptr->dlist < 1000) // já existe uma dlist ?
	{
			// Libera dlist existente
			glDeleteLists(ptr->dlist,1);
	}
	// O valor especial -2 indica que não queremos
	// gerar dlist para esse objeto
	ptr->dlist = -2;
}

// Função interna para criar uma display list
// (na verdade, a display list em si é gerada durante a
// primeira vez em que o objeto é redesenhado)
void _criaDList(OBJ *ptr)
{
	// Se o objeto não possui display list, gera uma identificação nova
	if(ptr->dlist == -1) ptr->dlist = glGenLists(1);
	// Adiciona 1000 ao valor, para informar à rotina de desenho
	// que uma nova display list deve ser compilada
	ptr->dlist = ptr->dlist + 1000;
}

// Cria uma display list para o objeto informado
// - se for NULL, cria display lists para TODOS os objetos
// (usada na rotina de desenho, se existir)
void CriaDisplayList(OBJ *ptr)
{
	if(ptr==NULL)
	{
		for(unsigned int i=0;i<_objetos.size();++i)
		{
			ptr = _objetos[i];
			// Pula os objetos que não devem usar dlists
			if(ptr->dlist == -2) continue;
			_criaDList(ptr);
		}
	}
	else if(ptr->dlist != -2)_criaDList(ptr);
}

// Decodifica uma imagem JPG e armazena-a em uma estrutura TEX.
void DecodificaJPG(jpeg_decompress_struct* cinfo, TEX *pImageData, bool inverte)
{
	// Lê o cabeçalho de um arquivo jpeg
	jpeg_read_header(cinfo, TRUE);
	
	// Começa a descompactar um arquivo jpeg com a informação 
	// obtida do cabeçalho
	jpeg_start_decompress(cinfo);

	// Pega as dimensões da imagem e varre as linhas para ler os dados do pixel
	pImageData->ncomp = cinfo->num_components;
	pImageData->dimx  = cinfo->image_width;
	pImageData->dimy  = cinfo->image_height;

	int rowSpan = pImageData->ncomp * pImageData->dimx;
	// Aloca memória para o buffer do pixel
	pImageData->data = new unsigned char[rowSpan * pImageData->dimy];
		
	// Aqui se usa a variável de estado da biblioteca cinfo.output_scanline 
	// como o contador de loop
	
	// Cria um array de apontadores para linhas
	int height = pImageData->dimy - 1;
	unsigned char** rowPtr = new unsigned char*[pImageData->dimy];
	if(inverte)
		for (int i = 0; i <= height; i++)
			rowPtr[height - i] = &(pImageData->data[i*rowSpan]);
	else
		for (int i = 0; i <= height; i++)
			rowPtr[i] = &(pImageData->data[i*rowSpan]);

	// Aqui se extrai todos os dados de todos os pixels
	int rowsRead = 0;
	while (cinfo->output_scanline < cinfo->output_height) 
	{
		// Lê a linha corrente de pixels e incrementa o contador de linhas lidas
		rowsRead += jpeg_read_scanlines(cinfo, &rowPtr[rowsRead], cinfo->output_height - rowsRead);
	}
	
	// Libera a memória
	delete [] rowPtr;

	// Termina a decompactação dos dados 
	jpeg_finish_decompress(cinfo);
}

// Carrega o arquivo JPG e retorna seus dados em uma estrutura tImageJPG.
TEX *CarregaJPG(const char *filename, bool inverte)
{
	struct jpeg_decompress_struct cinfo;
	TEX *pImageData = NULL;
	FILE *pFile;
	
	// Abre um arquivo JPG e verifica se não ocorreu algum problema na abertura
	if((pFile = fopen(filename, "rb")) == NULL) 
	{
		// Exibe uma mensagem de erro avisando que o arquivo não foi encontrado
		// e retorna NULL
		printf("Impossível carregar arquivo JPG: %s\n",filename);
		return NULL;
	}
	
	// Cria um gerenciado de erro
	jpeg_error_mgr jerr;

	// Objeto com informações de compactação para o endereço do gerenciador de erro
	cinfo.err = jpeg_std_error(&jerr);
	
	// Inicializa o objeto de decompactação
	jpeg_create_decompress(&cinfo);
	
	// Especifica a origem dos dados (apontador para o arquivo)	
	jpeg_stdio_src(&cinfo, pFile);
	
	// Aloca a estrutura que conterá os dados jpeg
	pImageData = (TEX*)malloc(sizeof(TEX));

	// Decodifica o arquivo JPG e preenche a estrutura de dados da imagem
	DecodificaJPG(&cinfo, pImageData, inverte);
	
	// Libera a memória alocada para leitura e decodificação do arquivo JPG
	jpeg_destroy_decompress(&cinfo);
	
	// Fecha o arquivo 
	fclose(pFile);

	// Retorna os dados JPG (esta memória deve ser liberada depois de usada)
	return pImageData;
}

