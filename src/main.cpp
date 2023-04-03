#include <clang/AST/AST.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;

static llvm::cl::OptionCategory ClassAnalyzerCategory("Class Analyzer Options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

class ClassAnalyzerASTVisitor : public RecursiveASTVisitor<ClassAnalyzerASTVisitor> {
    protected:
        CompilerInstance &mCI;

    public:
        explicit ClassAnalyzerASTVisitor(CompilerInstance &CI) : mCI(CI) {}

        bool VisitCXXMethodDecl(CXXMethodDecl *Decl) {
            const CXXRecordDecl *Parent = Decl->getParent();
            if (Parent == nullptr) {
                return true;
            }
            if (!Parent->getName().equals("Character")) {
                return true;
            }
            llvm::outs() << Decl->getName() << '\n';
            return true;
        }
};

class ClassAnalyzerASTConsumer : public ASTConsumer {
    protected:
        CompilerInstance &mCI;
        std::unique_ptr<ClassAnalyzerASTVisitor> mVisitor;

    public:
        explicit ClassAnalyzerASTConsumer(CompilerInstance &CI) : mCI(CI) {
            mVisitor = std::make_unique<ClassAnalyzerASTVisitor>(mCI);
        }

        void HandleTranslationUnit(ASTContext &Context) override {
            mVisitor->TraverseDecl(Context.getTranslationUnitDecl());
        }
};

class ClassAnalyzerMethodMatchCallback : public MatchFinder::MatchCallback {
    public:
        void run(const MatchFinder::MatchResult &Result) override {
            const auto *Method = Result.Nodes.getNodeAs<CXXMethodDecl>("method");
            const auto *Parent = Result.Nodes.getNodeAs<CXXRecordDecl>("parent");
            auto &DiagEng = Result.Context->getDiagnostics();
            if (Parent->getName().equals("Character")) {
                auto DiagID = DiagEng.getCustomDiagID(DiagnosticsEngine::Remark, "Class method %0");
                auto DiagBuilder = DiagEng.Report(Method->getOuterLocStart(), DiagID);
                DiagBuilder.AddString(Method->getName());
                DiagBuilder.AddSourceRange(CharSourceRange::getCharRange(Method->getSourceRange()));
            }
        }
};

class ClassAnalyzerFrontendAction : public ASTFrontendAction {
    public:
        ClassAnalyzerFrontendAction() {}

        auto CreateASTConsumer(CompilerInstance &CI, StringRef InFile)
            -> std::unique_ptr<ASTConsumer> override {
            auto Matcher = cxxMethodDecl(hasParent(cxxRecordDecl().bind("parent"))).bind("method");
            mMatchFinder.addMatcher(Matcher, &mCallback);
            return mMatchFinder.newASTConsumer();
        }

    private:
        MatchFinder mMatchFinder;
        ClassAnalyzerMethodMatchCallback mCallback;
};

int main(int argc, const char **argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, ClassAnalyzerCategory);
    if (!ExpectedParser) {
        llvm::errs() << ExpectedParser.takeError();
        return 1;
    }
    CommonOptionsParser &OptionsParser = ExpectedParser.get();
    ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
    return Tool.run(newFrontendActionFactory<ClassAnalyzerFrontendAction>().get());
}
